#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <stdexcept>
#include <errno.h>
#include <string.h>
#include <p2p-resource-sync/tcp_server.hpp>
#include <p2p-resource-sync/protocol.hpp>
#include <vector>
#include <thread>
#include <atomic>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <csignal>
#include <algorithm>

namespace p2p {

static std::atomic<bool> should_stop{false};
static void signal_handler(int signal) {
    should_stop = true;
}

int TcpServer::initializeSocket_(int port, int max_clients) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        throw std::runtime_error("Failed opening stream socket: " + std::string(strerror(errno)));
    }
    
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(sock);
        throw std::runtime_error("Failed setsockopt: " + std::string(strerror(errno)));
    }
    struct sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        close(sock);
        throw std::runtime_error("Binding socket failed: " + std::string(strerror(errno)));
    }
    if (listen(sock, max_clients) == -1) {
        close(sock);
        throw std::runtime_error("Failed listen: " + std::string(strerror(errno)));
    }
    return sock;
}

void TcpServer::handleClient_(int client_socket) {
   try {
       uint32_t messageLength;
       if (recv(client_socket, &messageLength, sizeof(messageLength), 0) <= 0) {
           throw std::runtime_error("Failed to receive message length");
       }

       auto request = std::unique_ptr<ResourceRequest>(
           static_cast<ResourceRequest*>(operator new(messageLength))
       );
       request->messageLength = messageLength;
       size_t remaining = messageLength - sizeof(messageLength);
       size_t offset = sizeof(messageLength);
       while (remaining > 0) {
           ssize_t received = recv(client_socket, 
                                 reinterpret_cast<char*>(request.get()) + offset,
                                 remaining, 0);
           if (received <= 0) {
               throw std::runtime_error("Failed to receive request data");
           }
           remaining -= received;
           offset += received;
       }

       auto resource_path = resource_manager_->getResourcePath(
           std::string(request->resourceName, request->resourceNameLength)
       );
       
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

       char buffer[4096];
       while (file.read(buffer, sizeof(buffer))) {
           if (send(client_socket, buffer, sizeof(buffer), 0) <= 0) {
               throw std::runtime_error("Failed to send file data");
           }
       }
       
       auto lastChunkSize = file.gcount();
       if (lastChunkSize > 0) {
           if (send(client_socket, buffer, lastChunkSize, 0) <= 0) {
               throw std::runtime_error("Failed to send last chunk");
           }
       }

   } catch (const std::exception& e) {
       close(client_socket);
       throw;
   }
   close(client_socket);
}

void TcpServer::run() {
   try {
       std::vector<std::jthread> client_threads;
       struct sockaddr_in client{};
       socklen_t client_length = sizeof(client);

       // Install signal handlers
       std::signal(SIGINT, signal_handler);
       std::signal(SIGTERM, signal_handler);
       should_stop = false;
       
       int server_socket = initializeSocket_(port_, max_clients_);
       if (server_socket < 0) {
           throw std::runtime_error("Failed to initialize socket");
       }

       std::cout << "TCP Server listening on port " << port_ << std::endl;
       std::cout << "Waiting for clients..." << std::endl;

       while (!should_stop) {
           int client_socket = accept(server_socket, 
                                    reinterpret_cast<struct sockaddr*>(&client), 
                                    &client_length);
                                    
           if (client_socket == -1) {
               if (errno == EINTR) {
                   continue;
               }
               std::cerr << "Failed to accept connection: " 
                        << strerror(errno) << std::endl;
               continue;
           }

           try {
               client_threads.emplace_back([this, client_socket]() {
                   try {
                       handleClient_(client_socket);
                   }
                   catch (const std::exception& e) {
                       std::cerr << "Client handler error: " 
                                << e.what() << std::endl;
                   }
               });

               client_threads.erase(
                   std::remove_if(client_threads.begin(), 
                                client_threads.end(),
                                [](const std::jthread& t) { 
                                    return !t.joinable(); 
                                }),
                   client_threads.end()
               );

           } catch (const std::exception& e) {
               std::cerr << "Failed to create client thread: " 
                        << e.what() << std::endl;
               close(client_socket);
           }
       }

       for (auto& thread : client_threads) {
           if (thread.joinable()) {
               thread.join();
           }
       }

       close(server_socket);
   }
   catch (const std::exception& e) {
       std::cerr << "Server error: " << e.what() << std::endl;
       throw;
   }
}

} // namespace p2p
