#include "p2p-resource-sync/constants.hpp"
#include "p2p-resource-sync/local_resource_manager.hpp"
#include "p2p-resource-sync/logger.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <p2p-resource-sync/announcement_broadcaster.hpp>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace p2p {

AnnouncementBroadcaster::AnnouncementBroadcaster(
    const std::shared_ptr<LocalResourceManager> &resource_manager,
    const uint32_t node_id, const uint16_t port, const uint16_t broadcast_port,
    const std::chrono::seconds broadcast_interval) {
  this->resource_manager_ = resource_manager;
  this->node_id_ = node_id;
  this->port_ = port;
  this->broadcast_interval_ = broadcast_interval;
  this->initializeSocket_(broadcast_port);
};

AnnouncementBroadcaster::~AnnouncementBroadcaster() { close(this->socket_); };

void AnnouncementBroadcaster::initializeSocket_(uint16_t broadcast_port) {
  this->socket_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (this->socket_ < 0) {
    throw std::runtime_error("Failed to create socket: " +
                             std::string(strerror(errno)));
  }

  int broadcast_enable =
      constants::announcement_broadcaster::socket::BROADCAST_ENABLE;
  if (setsockopt(this->socket_, SOL_SOCKET, SO_BROADCAST, &broadcast_enable,
                 sizeof(broadcast_enable)) < 0) {
    close(this->socket_);
    throw std::runtime_error("Failed to set broadcast option: " +
                             std::string(strerror(errno)));
  }

  this->broadcast_address_.sin_family = AF_INET;
  this->broadcast_address_.sin_port = htons(broadcast_port);
  this->broadcast_address_.sin_addr.s_addr = INADDR_BROADCAST;

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(this->port_);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(this->socket_, reinterpret_cast<struct sockaddr *>(&addr),
           sizeof(addr)) < 0) {
    close(this->socket_);
    throw std::runtime_error("Failed to bind socket: " +
                             std::string(strerror(errno)));
  }
};

AnnounceMessage AnnouncementBroadcaster::createAnnounceMessage_() const {
  AnnounceMessage message;
  auto local_resources = this->resource_manager_->getAllResources();
  message.timestamp =
      std::chrono::system_clock::now().time_since_epoch().count();
  message.senderId = this->node_id_;
  message.resourceCount = local_resources.size();
  message.resources.reserve(local_resources.size());

  for (const auto &[name, info] : local_resources) {
    Resource resource;
    resource.name = info.name;
    resource.size = info.size;
    message.resources.push_back(resource);
  }

  message.datagramLength = sizeof(uint32_t) + // datagramLength
                           sizeof(uint64_t) + // timestamp
                           sizeof(uint32_t) + // senderId
                           sizeof(uint32_t);  // resourceCount

  // add each resource
  for (const auto &resource : message.resources) {
    message.datagramLength += sizeof(uint32_t) +       // nameLength
                              resource.name.length() + // name
                              sizeof(uint32_t);        // resourceSize
  }
  return message;
};

void AnnouncementBroadcaster::broadcastAnnouncement_() const {
  AnnounceMessage message = this->createAnnounceMessage_();
  if (message.resourceCount < 1) {
    return;
  }
  std::vector<uint8_t> buffer;

  buffer.insert(buffer.end(),
                reinterpret_cast<uint8_t *>(&message.datagramLength),
                reinterpret_cast<uint8_t *>(&message.datagramLength) +
                    sizeof(message.datagramLength));
  buffer.insert(buffer.end(), reinterpret_cast<uint8_t *>(&message.timestamp),
                reinterpret_cast<uint8_t *>(&message.timestamp) +
                    sizeof(message.timestamp));
  buffer.insert(buffer.end(), reinterpret_cast<uint8_t *>(&message.senderId),
                reinterpret_cast<uint8_t *>(&message.senderId) +
                    sizeof(message.senderId));
  buffer.insert(buffer.end(),
                reinterpret_cast<uint8_t *>(&message.resourceCount),
                reinterpret_cast<uint8_t *>(&message.resourceCount) +
                    sizeof(message.resourceCount));

  for (const auto &resource : message.resources) {
    uint32_t nameLength = resource.name.length();
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t *>(&nameLength),
                  reinterpret_cast<uint8_t *>(&nameLength) +
                      sizeof(nameLength));
    buffer.insert(buffer.end(), resource.name.begin(), resource.name.end());
    buffer.insert(buffer.end(), (uint8_t *)&resource.size,
                  (uint8_t *)&resource.size + sizeof(resource.size));
  }

  if (sendto(this->socket_, buffer.data(), buffer.size(), 0,
             (struct sockaddr *)&this->broadcast_address_,
             sizeof(this->broadcast_address_)) == -1) {
    throw std::runtime_error("Failed to broadcast");
  }
  Logger::log(LogLevel::INFO,
              "Successfully broadcasted announcement message, size: " +
                  std::to_string(buffer.size()) + " bytes");
};

void AnnouncementBroadcaster::run() {
  this->running_ = true;
  while (this->running_) {
    try {
      this->broadcastAnnouncement_();
    } catch (const std::exception &e) {
      Logger::log(LogLevel::ERROR, "Broadcast error: " + std::string(e.what()));
    }
    std::this_thread::sleep_for(this->broadcast_interval_);
  }
};

void AnnouncementBroadcaster::stop() { this->running_ = false; };

} // namespace p2p
