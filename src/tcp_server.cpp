#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <csignal>
#include <errno.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <p2p-resource-sync/logger.hpp>
#include <p2p-resource-sync/protocol.hpp>
#include <p2p-resource-sync/tcp_server.hpp>
#include <stdexcept>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace p2p {
void TcpServer::stop() {
  should_stop_ = true;
  if (server_socket_ != -1) {
    shutdown(server_socket_, SHUT_RDWR);
    close(server_socket_);
  }
}

int TcpServer::initializeSocket_(int port, int max_clients) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    throw std::runtime_error("Failed opening stream socket: " +
                             std::string(strerror(errno)));
  }

  int reuseaddr = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                 sizeof(reuseaddr)) == -1) {
    close(sock);
    throw std::runtime_error("Failed to set SO_REUSEADDR: " +
                             std::string(strerror(errno)));
  }

  struct sockaddr_in server{};
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock, reinterpret_cast<struct sockaddr *>(&server), sizeof(server)) <
      0) {
    close(sock);
    throw std::runtime_error("Binding socket failed: " +
                             std::string(strerror(errno)));
  }
  if (listen(sock, max_clients) == -1) {
    close(sock);
    throw std::runtime_error("Failed listen: " + std::string(strerror(errno)));
  }
  return sock;
}

void TcpServer::simulatePeriodicDrop(size_t frequency) {
  should_simulate_periodic_drop_ = true;
  drop_frequency_ = frequency;
}

void TcpServer::sendChunk_(int client_socket, const char *data, size_t length,
                           size_t &total_sent) {
  size_t bytes_sent = 0;
  while (bytes_sent < length) {
    ssize_t sent =
        send(client_socket, data + bytes_sent, length - bytes_sent, 0);

    if (sent <= 0) {
      throw std::runtime_error("Failed to send data");
    }

    bytes_sent += sent;
    total_sent += sent;
  }
}

void TcpServer::handleClient_(int client_socket) {
  try {
    uint32_t messageLength;
    if (recv(client_socket, &messageLength, sizeof(messageLength), 0) <= 0) {
      throw std::runtime_error("Failed to receive message length");
    }

    auto request = std::unique_ptr<ResourceRequest>(
        static_cast<ResourceRequest *>(operator new(messageLength)));
    request->messageLength = messageLength;
    size_t remaining = messageLength - sizeof(messageLength);
    size_t offset = sizeof(messageLength);
    while (remaining > 0) {
      ssize_t received =
          recv(client_socket, reinterpret_cast<char *>(request.get()) + offset,
               remaining, 0);
      if (received <= 0) {
        throw std::runtime_error("Failed to receive request data");
      }
      remaining -= received;
      offset += received;
    }

    auto resource_path = resource_manager_->getResourcePath(
        std::string(request->resourceName, request->resourceNameLength));

    if (!resource_path) {
      uint8_t status = 0;
      if (send(client_socket, &status, sizeof(status), 0) <= 0) {
        throw std::runtime_error("Failed to send error status");
      }
      return;
    }

    std::ifstream file(*resource_path, std::ios::binary);
    if (!file) {
      throw std::runtime_error("Failed to open resource file");
    }

    file.seekg(request->offset);
    if (!file) {
      throw std::runtime_error("Invalid offset");
    }

    uint8_t status = 1;
    if (send(client_socket, &status, sizeof(status), 0) <= 0) {
      throw std::runtime_error("Failed to send success status");
    }

    auto size = std::filesystem::file_size(*resource_path);
    if (send(client_socket, &size, sizeof(size), 0) <= 0) {
      throw std::runtime_error("Failed to send file size");
    }

    size_t total_sent = 0;
    char buffer[constants::tcp_server::BUFFER_SIZE];

    int counter = 0;
    while (file.read(buffer, sizeof(buffer))) {
      sendChunk_(client_socket, buffer, sizeof(buffer), total_sent);
      if (should_simulate_periodic_drop_ &&
          counter % constants::tcp_server::DEFAULT_DROP_FREQUENCY ==
              constants::tcp_server::DEFAULT_DROP_FREQUENCY) {
        Logger::log(LogLevel::INFO,
                    "Simulating periodic connection drop after " +
                        std::to_string(total_sent) + " bytes");
        shutdown(client_socket, SHUT_RDWR);
        throw std::runtime_error("Simulated periodic connection drop");
      }
      counter++;
    }

    auto lastChunkSize = file.gcount();
    if (lastChunkSize > 0) {
      sendChunk_(client_socket, buffer, lastChunkSize, total_sent);
    }

  } catch (const std::exception &e) {
    close(client_socket);
    throw;
  }
  close(client_socket);
}

static void (*signal_handler(TcpServer *server))(int) {
  static TcpServer *srv = server;
  static int server_socket = -1;

  if (server != nullptr) {
    srv = server;
    server_socket = server->getServerSocket();
  }

  return [](int signal) {
    if (srv != nullptr) {
      srv->stop();
      if (server_socket != -1) {
        close(server_socket);
      }
    }
  };
}

int TcpServer::getServerSocket() { return server_socket_; };

void TcpServer::run() {
  try {
    std::vector<std::jthread> client_threads;
    struct sockaddr_in client{};
    socklen_t client_length = sizeof(client);
    auto handler = signal_handler(this);
    std::signal(SIGINT, handler);
    std::signal(SIGTERM, handler);

    server_socket_ = initializeSocket_(port_, max_clients_);
    if (server_socket_ < 0) {
      throw std::runtime_error("Failed to initialize socket");
    }

    while (!should_stop_) {
      std::this_thread::sleep_for(constants::tcp_server::MAIN_LOOP_DELAY_MS);
      int client_socket =
          accept(server_socket_, reinterpret_cast<struct sockaddr *>(&client),
                 &client_length);

      if (client_socket == -1 && !should_stop_) {
        if (errno == EINTR) {
          continue;
        }
        continue;
      }
      char client_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(client.sin_addr), client_ip, INET_ADDRSTRLEN);
      Logger::log(LogLevel::INFO,
                  "New connection from " + std::string(client_ip));

      if (!should_stop_) {
        try {
          client_threads.emplace_back([this, client_socket]() {
            try {
              handleClient_(client_socket);
            } catch (const std::exception &e) {
              Logger::log(LogLevel::ERROR,
                          std::string("Client handler error") + e.what());
            }
          });

          std::erase_if(client_threads,
                        [](const std::jthread &t) { return !t.joinable(); });

        } catch (const std::exception &e) {
          Logger::log(LogLevel::ERROR,
                      std::string("Failed to create client thread") + e.what());
          close(client_socket);
        }
      }
    }

    for (auto &thread : client_threads) {
      if (thread.joinable()) {
        thread.join();
      }
    }

    close(server_socket_);
  } catch (const std::exception &e) {
    Logger::log(LogLevel::ERROR, std::string("Server error:") + e.what());
    throw;
  }
}

} // namespace p2p
