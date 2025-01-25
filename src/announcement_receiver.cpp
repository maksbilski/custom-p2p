#include "p2p-resource-sync/announcement_receiver.hpp"
#include "p2p-resource-sync/logger.hpp"
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <p2p-resource-sync/announcement_broadcaster.hpp>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

namespace p2p {

AnnouncementReceiver::AnnouncementReceiver(
    std::shared_ptr<RemoteResourceManager> resource_manager, uint32_t node_id,
    uint16_t port, int socket_timeout_ms) {
  this->resource_manager_ = resource_manager;
  this->node_id_ = node_id;
  this->port_ = port;
  this->initializeSocket_(socket_timeout_ms);
};

AnnouncementReceiver::~AnnouncementReceiver() { close(this->socket_); };

void AnnouncementReceiver::initializeSocket_(int socket_timeout_ms) {
  this->socket_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (this->socket_ < 0) {
    throw std::runtime_error("Failed to create socket: " +
                             std::string(strerror(errno)));
  }

  struct timeval timeout;
  timeout.tv_sec = socket_timeout_ms / 1000;
  timeout.tv_usec = (socket_timeout_ms % 1000) * 1000;

  if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) <
      0) {
    close(socket_);
    throw std::runtime_error("Failed to set socket timeout: " +
                             std::string(strerror(errno)));
  }
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(this->port_);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(this->socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(this->socket_);
    throw std::runtime_error("Failed to bind socket: " +
                             std::string(strerror(errno)));
  }
};

void AnnouncementReceiver::receiveAndProcessAnnouncement_() {
  std::vector<uint8_t> buffer(MAX_DATAGRAM_SIZE);
  struct sockaddr_in sender_addr;
  socklen_t sender_addr_len = sizeof(sender_addr);

  ssize_t received =
      recvfrom(socket_, buffer.data(), buffer.size(), 0,
               (struct sockaddr *)&sender_addr, &sender_addr_len);

  if (received < 0) {
    return;
  }

  try {
    processAnnouncement_(buffer, received, sender_addr);
    Logger::log(LogLevel::INFO,
                "Successfully received announcement message, size: " +
                    std::to_string(buffer.size()) + " bytes");
  } catch (const std::exception &e) {
    throw std::runtime_error("Failed to process announcement");
  }
}

void AnnouncementReceiver::processAnnouncement_(
    const std::vector<uint8_t> &buffer, size_t size,
    const struct sockaddr_in &sender_addr) {

  AnnounceMessage message = parseAnnounceMessage_(buffer, size);

  if (message.senderId == this->node_id_)
    return;

  this->resource_manager_->addOrUpdateNodeResources(
      sender_addr, message.resources, message.timestamp);
}

AnnounceMessage
AnnouncementReceiver::parseAnnounceMessage_(const std::vector<uint8_t> &buffer,
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

  std::memcpy(&message.senderId, buffer.data() + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  std::memcpy(&message.resourceCount, buffer.data() + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  for (uint32_t i = 0; i < message.resourceCount; ++i) {
    Resource resource;

    uint32_t nameLength;
    std::memcpy(&nameLength, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    resource.name.assign(reinterpret_cast<const char *>(buffer.data() + offset),
                         nameLength);
    offset += nameLength;

    std::memcpy(&resource.size, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    message.resources.push_back(resource);
  }

  return message;
}

void AnnouncementReceiver::run() {
  this->running_ = true;
  while (this->running_) {
    try {
      this->receiveAndProcessAnnouncement_();
    } catch (std::exception e) {
      Logger::log(LogLevel::ERROR,
                  "Receiving Broadcast error: " + std::string(e.what()));
    }
  }
}

void AnnouncementReceiver::stop() { this->running_ = false; }

} // namespace p2p
