#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <p2p-resource-sync/resource_downloader.hpp>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace p2p {
ResourceDownloader::ResourceDownloader(const std::string &download_dir,
                                       uint32_t socket_timeout_ms)
    : download_dir_(download_dir), socket_timeout_ms_(socket_timeout_ms) {}

int ResourceDownloader::initialize_socket_(const std::string &host,
                                           int port) const {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    throw std::runtime_error("Error opening socket");
  }
  struct timeval timeout;
  timeout.tv_sec = this->socket_timeout_ms_ / 1000;
  timeout.tv_usec = (this->socket_timeout_ms_ % 1000) * 1000;

  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) <
      0) {
    close(sock);
    throw std::runtime_error("Error setting receive timeout");
  }

  if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) <
      0) {
    close(sock);
    throw std::runtime_error("Error setting send timeout");
  }

  struct sockaddr_in server;

  server.sin_family = AF_INET;
  const struct hostent *hp = gethostbyname(host.c_str());

  if (!hp) {
    close(sock);
    throw std::runtime_error("Error resolving hostname");
  }
  std::memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
  server.sin_port = htons(static_cast<uint16_t>(port));

  if (connect(sock, reinterpret_cast<struct sockaddr *>(&server), sizeof(server)) < 0) {
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

uint64_t ResourceDownloader::receive_file_(int sock, uint64_t offset,
                                           const std::string &resource_name,
                                           uint64_t file_size) const {
  std::filesystem::path file_path =
      std::filesystem::path(download_dir_) / resource_name;

  std::ofstream file(file_path,
                     std::ios::binary |
                         (offset > 0 ? std::ios::app : std::ios::trunc));
  if (!file) {
    throw std::runtime_error("Failed to create output file");
  }

  if (offset > 0) {
    file.seekp(offset);
  }

  uint64_t total_received = offset;
  int last_percentage = -1;

  while (total_received < file_size) {
    char buffer[constants::resource_downloader::BUFFER_SIZE];
    size_t to_receive = std::min(
        sizeof(buffer), static_cast<size_t>(file_size - total_received));

    ssize_t received = recv(sock, buffer, to_receive, 0);
    if (received <= 0) {
      return total_received;
    }

    file.write(buffer, received);
    if (!file) {
      throw std::runtime_error("Failed to write to output file");
    }

    total_received += received;

    int current_percentage =
        static_cast<int>((total_received * 100) / file_size);
    if (current_percentage != last_percentage) {
      std::cout << "\rDownloading: " << total_received << "/" << file_size
                << " bytes (" << current_percentage << "%)    " << std::flush;
      last_percentage = current_percentage;
    }
  }
  return total_received;
}

std::pair<uint64_t, uint64_t>
ResourceDownloader::downloadResource(const std::string &peer_addr,
                                     int peer_port, uint64_t offset,
                                     const std::string &resource_name) const {
  std::cout << "Downloading " << resource_name << " from " << peer_addr
            << std::endl;
  uint64_t current_offset = offset;
  uint64_t file_size = 0;

  for (int attempt = 0; attempt < constants::resource_downloader::MAX_RETRIES; attempt++) {

    if (attempt > 0) {
      std::cout << "Retrying download (attempt " << attempt + 1 << "/"
                << constants::resource_downloader::MAX_RETRIES << ") from offset " << current_offset
                << std::endl;
    }

    try {
      int sock = initialize_socket_(peer_addr, peer_port);
      send_resource_request_(sock, current_offset, resource_name);

      auto [exists, size] = receive_initial_response_(sock);
      if (!exists) {
        close(sock);
        return {0, 0};
      }

      file_size = size;
      current_offset =
          receive_file_(sock, current_offset, resource_name, file_size);
      close(sock);

      if (current_offset == file_size) {
        return {current_offset, file_size};
      }
      std::cout << "Download incomplete (attempt " << attempt + 1
                << "): " << current_offset << "/" << file_size << " bytes"
                << std::endl;

    } catch (const std::exception &e) {
      std::cerr << "Error during download (attempt " << attempt + 1
                << "): " << e.what() << std::endl;
        throw;
      }
    }
  return {current_offset, file_size};
  }
}

