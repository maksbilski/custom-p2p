#pragma once

#include "announcement_broadcaster.hpp"
#include "remote_resource_manager.hpp"
#include "constants.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <netinet/in.h>

namespace p2p {

class AnnouncementReceiver {
public:
  AnnouncementReceiver(std::shared_ptr<RemoteResourceManager> resource_manager,
                       uint32_t node_id, uint16_t port,
                       int socket_timeout_ms = constants::announcement_receiver::DEFAULT_SOCKET_TIMEOUT_MS);

  ~AnnouncementReceiver();

  AnnouncementReceiver(const AnnouncementReceiver &) = delete;
  AnnouncementReceiver &operator=(const AnnouncementReceiver &) = delete;

  void run();
  void stop();

private:
  void initializeSocket_(int socket_timeout_ms);

  bool receiveAndProcessAnnouncement_();

  void processAnnouncement_(const std::vector<uint8_t> &buffer, size_t size,
                            const struct sockaddr_in &sender_addr);

  AnnounceMessage parseAnnounceMessage_(const std::vector<uint8_t> &buffer,
                                        size_t size);

  std::shared_ptr<RemoteResourceManager> resource_manager_;
  uint32_t node_id_;
  uint16_t port_;
  int socket_;
  std::atomic<bool> running_{false};
  static constexpr size_t MAX_DATAGRAM_SIZE =
      constants::announcement_receiver::MAX_DATAGRAM_SIZE;
};

} // namespace p2p
