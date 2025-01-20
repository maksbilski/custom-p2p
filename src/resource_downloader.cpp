#include <arpa/inet.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <p2p-resource-sync/resource_downloader.hpp>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace p2p {
ResourceDownloader::ResourceDownloader(const std::string &download_dir,
                                       uint32_t connect_timeout_ms)
    : download_dir_(download_dir), connect_timeout_ms_(connect_timeout_ms) {}

int ResourceDownloader::initialize_socket_(const std::string &host,
                                           int port) const {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    throw std::runtime_error("Error opening socket");
  }
  struct sockaddr_in server;
  struct hostent *hp;

  server.sin_family = AF_INET;
  hp = gethostbyname(host.c_str());

  if (!hp) {
    close(sock);
    throw std::runtime_error("Error resolving hostname");
  }
  std::memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
  server.sin_port = htons(static_cast<uint16_t>(port));

  if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
    close(sock);
    throw std::runtime_error("Error connecting to server");
  }
  return sock;
}

std::unique_ptr<ResourceRequest> ResourceDownloader::create_resource_request_(
    const uint64_t offset, const std::string &resource_name) const {
  const uint32_t name_length = static_cast<uint32_t>(resource_name.length());
  const uint32_t message_length = sizeof(ResourceRequest) + name_length;

  uint8_t *buffer = new uint8_t[message_length];
  ResourceRequest *request = reinterpret_cast<ResourceRequest *>(buffer);

  request->messageLength = message_length;
  request->resourceNameLength = name_length;
  request->offset = offset;
  std::memcpy(request->resourceName, resource_name.c_str(), name_length);

  return std::unique_ptr<ResourceRequest>(request);
}

void ResourceDownloader::send_resource_request_(
    const int sock, const uint64_t offset,
    const std::string &resource_name) const {
  auto request = create_resource_request_(offset, resource_name);

  size_t total_sent = 0;
  const uint8_t *buffer = reinterpret_cast<const uint8_t *>(request.get());

  while (total_sent < request->messageLength) {
    ssize_t bytes_sent =
        send(sock, buffer + total_sent, request->messageLength - total_sent, 0);

    if (bytes_sent <= 0) {
      throw std::runtime_error("Failed to send resource request");
    }

    total_sent += bytes_sent;
  }
}

std::pair<bool, uint64_t>
ResourceDownloader::receive_initial_response_(int sock) const {
  uint8_t status;
  ssize_t received = recv(sock, &status, sizeof(status), 0);
  if (received <= 0) {
    throw std::runtime_error("Failed to receive status");
  }

  if (status == 0) {
    return {false, 0};
  }

  uint64_t file_size;
  received = recv(sock, &file_size, sizeof(file_size), 0);
  if (received <= 0) {
    throw std::runtime_error("Failed to receive file size");
  }

  return {true, file_size};
}

void ResourceDownloader::receive_file_(int sock,
                                       const std::string &resource_name,
                                       uint64_t file_size) const {
  std::filesystem::path file_path =
      std::filesystem::path(download_dir_) / resource_name;

  std::ofstream file(file_path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to create output file");
  }

  char buffer[4096];
  uint64_t total_received = 0;

  while (total_received < file_size) {
    size_t to_receive = std::min(
        sizeof(buffer), static_cast<size_t>(file_size - total_received));

    ssize_t received = recv(sock, buffer, to_receive, 0);
    if (received <= 0) {
      throw std::runtime_error("Failed to receive file data");
    }

    file.write(buffer, received);
    if (!file) {
      throw std::runtime_error("Failed to write to output file");
    }

    total_received += received;
  }
}

bool ResourceDownloader::downloadResource(const std::string &peer_addr,
                                          int peer_port,
                                          const std::string &resource_name) {
  std::cout << "Starting download of " << resource_name << " from " << peer_addr
            << ":" << peer_port << std::endl;

  int sock = initialize_socket_(peer_addr, peer_port);
  std::cout << "Socket initialized: " << sock << std::endl;

  try {
    std::cout << "Sending resource request..." << std::endl;
    send_resource_request_(sock, 0, resource_name);
    std::cout << "Resource request sent successfully" << std::endl;

    std::cout << "Waiting for initial response..." << std::endl;
    auto [exists, file_size] = receive_initial_response_(sock);
    std::cout << "Initial response received - Resource exists: "
              << (exists ? "yes" : "no") << ", Size: " << file_size << " bytes"
              << std::endl;

    if (!exists) {
      std::cout << "Resource does not exist, aborting download" << std::endl;
      close(sock);
      return false;
    }

    std::cout << "Starting file transfer..." << std::endl;
    receive_file_(sock, resource_name, file_size);
    std::cout << "File transfer completed successfully" << std::endl;

    close(sock);
    std::cout << "Socket closed, download finished" << std::endl;
    return true;
  } catch (const std::exception &e) {
    std::cerr << "Error during download: " << e.what() << std::endl;
    close(sock);
    throw;
  }
}

} // namespace p2p
