#pragma once
#include "local_resource_manager.hpp"
#include "remote_resource_manager.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>

namespace p2p {

typedef struct {
  uint32_t datagramLength;
  uint64_t timestamp;
  uint32_t senderId;
  uint32_t resourceCount;
  std::vector<Resource> resources;
} AnnounceMessage;

class AnnouncementBroadcaster {
public:
  AnnouncementBroadcaster(
      std::shared_ptr<LocalResourceManager> resource_manager, uint32_t node_id,
      uint16_t port, uint16_t broadcast_port,
      std::chrono::seconds broadcast_interval = std::chrono::seconds(30));

  ~AnnouncementBroadcaster();

  // Delete copy and move operations
  AnnouncementBroadcaster(const AnnouncementBroadcaster &) = delete;
  AnnouncementBroadcaster &operator=(const AnnouncementBroadcaster &) = delete;

  void run();
  void stop();

private:
  void initializeSocket_(uint16_t broadcast_port);

  AnnounceMessage createAnnounceMessage_() const;

  void broadcastAnnouncement_() const;

  std::shared_ptr<LocalResourceManager> resource_manager_;
  uint32_t node_id_;
  uint16_t port_;
  std::chrono::seconds broadcast_interval_;
  int socket_;
  struct sockaddr_in broadcast_address_;
  std::atomic<bool> running_{false};
};

} // namespace p2p
