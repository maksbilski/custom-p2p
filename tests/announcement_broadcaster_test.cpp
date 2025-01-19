#include "p2p-resource-sync/announcement_broadcaster.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <ctime>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>

class AnnouncementBroadcasterTest : public ::testing::Test {
protected:
  AnnouncementBroadcasterTest() : manager(), port(8000) {}

  p2p::LocalResourceManager manager;
  uint16_t port;

  void SetUp() override {}
  void TearDown() override {}

  std::vector<p2p::AnnounceMessage>
  receiveAnnouncements(std::chrono::milliseconds duration) {
    std::vector<p2p::AnnounceMessage> received;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
      throw std::runtime_error("Failed to create socket: " +
                               std::string(strerror(errno)));
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      close(sock);
      throw std::runtime_error("Failed to bind socket: " +
                               std::string(strerror(errno)));
    }

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < duration) {
    }

    close(sock);
    return received;
  }
};
