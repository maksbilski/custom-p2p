#include "p2p-resource-sync/announcement_receiver.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <p2p-resource-sync/announcement_broadcaster.hpp>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace p2p {

AnnouncementReceiver::AnnouncementReceiver(
    std::shared_ptr<RemoteResourceManager> resource_manager, uint16_t port,
    int socket_timeout, bool reuse_port) {
  this->resource_manager_ = resource_manager;
  this->port_ = port;
  this->initializeSocket(socket_timeout, reuse_port);
};

AnnouncementReceiver::~AnnouncementReceiver() { close(this->socket_); };

void AnnouncementReceiver::initializeSocket(int socket_timeout,
                                            bool reuse_port) {
  this->socket_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (this->socket_ < 0) {
    throw std::runtime_error("Failed to create socket: " +
                             std::string(strerror(errno)));
  }

  if (reuse_port) {
    int reuse = 1;
    if (setsockopt(this->socket_, SOL_SOCKET, SO_REUSEPORT, &reuse,
                   sizeof(reuse)) < 0) {
      close(this->socket_);
      throw std::runtime_error("Failed to set SO_REUSEPORT: " +
                               std::string(strerror(errno)));
    }
  }

  struct timeval tv;
  tv.tv_sec = socket_timeout;
  tv.tv_usec = 0;
  if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    close(socket_);
    throw std::runtime_error("Failed to set socket timeout: " +
                             std::string(strerror(errno)));
  }
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(this->socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(this->socket_);
    throw std::runtime_error("Failed to bind socket: " +
                             std::string(strerror(errno)));
  }
};

bool AnnouncementReceiver::receiveAndProcessAnnouncement() {
  std::vector<uint8_t> buffer(MAX_DATAGRAM_SIZE);
  struct sockaddr_in sender_addr;
  socklen_t sender_addr_len = sizeof(sender_addr);

  ssize_t received =
      recvfrom(socket_, buffer.data(), buffer.size(), 0,
               (struct sockaddr *)&sender_addr, &sender_addr_len);

  if (received < 0) {
    return false;
  }

  try {
    processAnnouncement(buffer, received, sender_addr);
    return true;
  } catch (const std::exception &e) {
    return false;
  }
}

void AnnouncementReceiver::processAnnouncement(
    const std::vector<uint8_t> &buffer, size_t size,
    const struct sockaddr_in &sender_addr) {
  AnnounceMessage message = parseAnnounceMessage(buffer, size);

  this->resource_manager_->addOrUpdateNodeResources(
      sender_addr, message.resources, message.timestamp);
}

AnnounceMessage
AnnouncementReceiver::parseAnnounceMessage(const std::vector<uint8_t> &buffer,
                                           size_t size) {
  if (size < sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t)) {
    throw std::runtime_error("Invalid datagram header");
  }

  size_t offset = 0;
  AnnounceMessage message;

  std::memcpy(&message.datagramLength, buffer.data() + offset,
              sizeof(uint32_t));
  offset += sizeof(uint32_t);

  if (message.datagramLength != size) {
    throw std::runtime_error("Invalid datagram length");
  }

  std::memcpy(&message.timestamp, buffer.data() + offset, sizeof(uint64_t));
  offset += sizeof(uint64_t);

  std::memcpy(&message.resourceCount, buffer.data() + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  for (uint32_t i = 0; i < message.resourceCount; ++i) {
    Resource resource;

    uint32_t nameLength;
    if (offset + sizeof(uint32_t) > size) {
      throw std::runtime_error("Buffer overflow reading name length");
    }
    std::memcpy(&nameLength, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (offset + nameLength > size) {
      throw std::runtime_error("Buffer overflow reading name");
    }
    resource.name.assign(reinterpret_cast<const char *>(buffer.data() + offset),
                         nameLength);
    offset += nameLength;

    if (offset + sizeof(uint32_t) > size) {
      throw std::runtime_error("Buffer overflow reading resource size");
    }
    std::memcpy(&resource.size, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    message.resources.push_back(resource);
  }

  return message;
}

void AnnouncementReceiver::run() {
  this->running_ = true;
  while (this->running_) {
    this->receiveAndProcessAnnouncement();
  }
}

void AnnouncementReceiver::stop() { this->running_ = false; }

} // namespace p2p
