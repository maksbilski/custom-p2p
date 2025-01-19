#include "p2p-resource-sync/announcement_broadcaster.hpp"
#include "p2p-resource-sync/local_resource_manager.hpp"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <ostream>
#include <sys/socket.h>
#include <thread>
#include <vector>

class AnnouncementBroadcasterTest : public ::testing::Test {
protected:
  AnnouncementBroadcasterTest() : port(8001), recv_port(8002) {}

  std::shared_ptr<p2p::LocalResourceManager> manager_ref =
      std::make_shared<p2p::LocalResourceManager>();
  uint16_t port;
  uint16_t recv_port;

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
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
      close(sock);
      throw std::runtime_error("Failed to set SO_REUSEADDR: " +
                               std::string(strerror(errno)));
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      close(sock);
      throw std::runtime_error("Failed to bind socket: " +
                               std::string(strerror(errno)));
    }

    auto start = std::chrono::steady_clock::now();
    char buffer[4096];
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);

    while (std::chrono::steady_clock::now() - start < duration) {
      ssize_t received_bytes =
          recvfrom(sock, buffer, sizeof(buffer), 0,
                   (struct sockaddr *)&sender_addr, &sender_addr_len);

      if (received_bytes > 0) {
        p2p::AnnounceMessage message;

        std::memcpy(&message.datagramLength, buffer, sizeof(uint32_t));
        size_t offset = sizeof(uint32_t);

        std::memcpy(&message.timestamp, buffer + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);

        std::memcpy(&message.resourceCount, buffer + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        for (uint32_t i = 0; i < message.resourceCount; i++) {
          p2p::Resource resource;

          std::memcpy(&resource.nameLength, buffer + offset, sizeof(uint32_t));
          offset += sizeof(uint32_t);

          resource.name.assign(buffer + offset, resource.nameLength);
          offset += resource.nameLength;

          std::memcpy(&resource.size, buffer + offset, sizeof(uint32_t));
          offset += sizeof(uint32_t);

          message.resources.push_back(resource);
        }
        received.push_back(message);
      }
    }

    close(sock);
    return received;
  }
};

TEST_F(AnnouncementBroadcasterTest, CanBeInstantiated) {
  EXPECT_NO_THROW(
      { p2p::AnnouncementBroadcaster broadcaster(manager_ref, port); });
}

TEST_F(AnnouncementBroadcasterTest, CannotBeCopied) {
  EXPECT_FALSE(std::is_copy_constructible_v<p2p::AnnouncementBroadcaster>);
  EXPECT_FALSE(std::is_copy_assignable_v<p2p::AnnouncementBroadcaster>);
}

TEST_F(AnnouncementBroadcasterTest, NoBroadcastWhenNoResources) {
  p2p::AnnouncementBroadcaster broadcaster(manager_ref, port,
                                           std::chrono::seconds(2));

  std::jthread broadcast_thread([&broadcaster]() { broadcaster.run(); });

  auto messages = receiveAnnouncements(std::chrono::milliseconds(4000));
  broadcaster.stop();
  broadcast_thread.join();

  EXPECT_TRUE(messages.empty());
}

TEST_F(AnnouncementBroadcasterTest, BroadcastsMessageWithResource) {
  p2p::AnnouncementBroadcaster broadcaster(manager_ref, port,
                                           std::chrono::seconds(2));
  manager_ref->addResource("test", "../test_local_files/test.txt");

  std::jthread broadcast_thread([&broadcaster]() { broadcaster.run(); });

  auto messages = receiveAnnouncements(std::chrono::milliseconds(4000));
  broadcaster.stop();
  broadcast_thread.join();

  ASSERT_EQ(messages.size(), 3);
}
