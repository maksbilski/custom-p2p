#include "p2p-resource-sync/announcement_broadcaster.hpp"
#include "p2p-resource-sync/announcement_receiver.hpp"
#include "p2p-resource-sync/local_resource_manager.hpp"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <gtest/gtest.h>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <vector>

class AnnouncementTest : public ::testing::Test {
protected:
  AnnouncementTest() : sender_port(8001), brdcst_port(8002) {}

  std::shared_ptr<p2p::LocalResourceManager> local_manager_ref =
      std::make_shared<p2p::LocalResourceManager>();
  std::shared_ptr<p2p::RemoteResourceManager> remote_manager_ref =
      std::make_shared<p2p::RemoteResourceManager>(std::chrono::seconds(1));
  uint16_t sender_port;
  uint16_t brdcst_port;

  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(AnnouncementTest, BroadcasterCanBeInstantiated) {
  EXPECT_NO_THROW({
    p2p::AnnouncementBroadcaster broadcaster(local_manager_ref, 1, sender_port,
                                             brdcst_port);
  });
}

TEST_F(AnnouncementTest, ReceiverCanBeInstantiated) {
  EXPECT_NO_THROW({
    p2p::AnnouncementReceiver receiver(remote_manager_ref, 1, brdcst_port);
  });
}

TEST_F(AnnouncementTest, CannotBeCopied) {
  EXPECT_FALSE(std::is_copy_constructible_v<p2p::AnnouncementBroadcaster>);
  EXPECT_FALSE(std::is_copy_assignable_v<p2p::AnnouncementBroadcaster>);
  EXPECT_FALSE(std::is_copy_constructible_v<p2p::AnnouncementReceiver>);
  EXPECT_FALSE(std::is_copy_assignable_v<p2p::AnnouncementReceiver>);
}

TEST_F(AnnouncementTest, NoBroadcastWhenNoResources) {
  p2p::AnnouncementBroadcaster broadcaster(
      local_manager_ref, 1, sender_port, brdcst_port, std::chrono::seconds(2));
  p2p::AnnouncementReceiver receiver(remote_manager_ref, 2, brdcst_port, 1);

  auto remote_resources = remote_manager_ref->getAllResources();
  EXPECT_TRUE(remote_resources.empty());

  std::jthread broadcaster_thread([&broadcaster]() { broadcaster.run(); });
  std::jthread receiver_thread([&receiver]() { receiver.run(); });

  std::this_thread::sleep_for(std::chrono::seconds(4));

  broadcaster.stop();
  receiver.stop();
  broadcaster_thread.join();
  receiver_thread.join();

  remote_resources = remote_manager_ref->getAllResources();

  EXPECT_TRUE(remote_resources.empty());
}

TEST_F(AnnouncementTest, BroadcastsMessageWithResource) {
  p2p::AnnouncementBroadcaster broadcaster(
      local_manager_ref, 1, sender_port, brdcst_port, std::chrono::seconds(2));
  p2p::AnnouncementReceiver receiver(remote_manager_ref, 2, brdcst_port, 1);
  local_manager_ref->addResource("test", "../test_local_files/test.txt");

  auto remote_resources = remote_manager_ref->getAllResources();
  EXPECT_TRUE(remote_resources.empty());

  std::jthread broadcaster_thread([&broadcaster]() { broadcaster.run(); });
  std::jthread receiver_thread([&receiver]() { receiver.run(); });

  std::this_thread::sleep_for(std::chrono::seconds(4));

  broadcaster.stop();
  receiver.stop();
  broadcaster_thread.join();
  receiver_thread.join();

  remote_resources = remote_manager_ref->getAllResources();
  EXPECT_TRUE(remote_resources.empty());
  for (const auto &[addr, resource] : remote_resources) {
    std::cout << "Address: " << inet_ntoa(addr.sin_addr) << ":"
              << ntohs(addr.sin_port) << " Resource: " << resource.name
              << " Size: " << resource.size << std::endl;
  }
}
