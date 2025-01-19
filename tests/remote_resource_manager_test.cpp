#include "p2p-resource-sync/remote_resource_manager.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <ctime>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <vector>

class RemoteResourceManagerTest : public ::testing::Test {
protected:
  RemoteResourceManagerTest() : manager(std::chrono::seconds(2)) {}

  p2p::RemoteResourceManager manager;

  void SetUp() override {}

  void TearDown() override {}

  struct sockaddr_in createAddress(const char *ip, uint16_t port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    return addr;
  }

  void addFreshNode(const sockaddr_in &addr) {
    std::vector<p2p::RemoteResource> resources = {{"test.txt", 1000}};
    manager.addOrUpdateNodeResources(addr, resources);
  }
};

TEST_F(RemoteResourceManagerTest, CannotBeCopied) {
  EXPECT_FALSE(std::is_copy_constructible_v<p2p::RemoteResourceManager>);
  EXPECT_FALSE(std::is_copy_assignable_v<p2p::RemoteResourceManager>);
}

TEST_F(RemoteResourceManagerTest, EmptyManagerHasNoResources) {
  EXPECT_TRUE(manager.getAllResources().empty());
}

TEST_F(RemoteResourceManagerTest, CanAddSingleResourceToNode) {
  struct sockaddr_in node_address1 = this->createAddress("192.168.1.1", 8000);

  this->addFreshNode(node_address1);

  EXPECT_FALSE(manager.getAllResources().empty());
}

TEST_F(RemoteResourceManagerTest, CanAddMultipleResourcesToNode) {
  struct sockaddr_in node_address1 = this->createAddress("192.168.1.1", 8000);
  p2p::RemoteResource resource1{.name = "test_resource1", .size = 100};
  p2p::RemoteResource resource2{.name = "test_resource2", .size = 100};

  manager.addOrUpdateNodeResources(node_address1, {resource1, resource2});

  ASSERT_EQ(manager.getAllResources().size(), 2);
}

TEST_F(RemoteResourceManagerTest, CanAddMultipleNodesWithResources) {
  struct sockaddr_in node_address1 = this->createAddress("192.168.1.1", 8000);
  struct sockaddr_in node_address2 = this->createAddress("192.168.1.2", 8000);
  p2p::RemoteResource resource1{.name = "test_resource1", .size = 100};
  p2p::RemoteResource resource2{.name = "test_resource2", .size = 100};

  manager.addOrUpdateNodeResources(node_address1, {resource1, resource2});
  manager.addOrUpdateNodeResources(node_address2, {resource1, resource2});

  ASSERT_EQ(manager.getAllResources().size(), 4);
}

TEST_F(RemoteResourceManagerTest, NodeHasResource) {
  struct sockaddr_in node_address1 = this->createAddress("192.168.1.1", 8000);

  this->addFreshNode(node_address1);

  EXPECT_TRUE(manager.hasResource(node_address1, "test.txt"));
}

TEST_F(RemoteResourceManagerTest, NodeHasNoResource) {
  struct sockaddr_in node_address1 = this->createAddress("192.168.1.1", 8000);

  this->addFreshNode(node_address1);

  EXPECT_FALSE(manager.hasResource(node_address1, "test_resource"));
}

TEST_F(RemoteResourceManagerTest, FindNodeWithResource) {
  struct sockaddr_in node_address1 = this->createAddress("192.168.1.1", 8000);
  this->addFreshNode(node_address1);

  std::vector<struct sockaddr_in> found_nodes =
      manager.findNodesWithResource("test.txt");

  EXPECT_FALSE(found_nodes.empty());
}

TEST_F(RemoteResourceManagerTest, FindMultipleNodesWithResource) {
  struct sockaddr_in node_address1 = this->createAddress("192.168.1.1", 8000);
  struct sockaddr_in node_address2 = this->createAddress("192.168.1.2", 8000);
  this->addFreshNode(node_address1);
  this->addFreshNode(node_address2);

  std::vector<struct sockaddr_in> found_nodes =
      manager.findNodesWithResource("test.txt");

  ASSERT_EQ(found_nodes.size(), 2);
}

TEST_F(RemoteResourceManagerTest, NoNodesWithResourceFound) {
  struct sockaddr_in node_address1 = this->createAddress("192.168.1.1", 8000);
  struct sockaddr_in node_address2 = this->createAddress("192.168.1.2", 8000);
  this->addFreshNode(node_address1);
  this->addFreshNode(node_address2);

  std::vector<struct sockaddr_in> found_nodes =
      manager.findNodesWithResource("test1.txt");

  EXPECT_TRUE(found_nodes.empty());
}

TEST_F(RemoteResourceManagerTest, CleanupStaleNodesTest) {
  struct sockaddr_in node_address1 = this->createAddress("192.168.1.1", 8000);
  struct sockaddr_in node_address2 = this->createAddress("192.168.1.2", 8000);
  std::vector<p2p::RemoteResource> resources1 = {{"test1.txt", 1000}};
  std::vector<p2p::RemoteResource> resources2 = {{"test2.txt", 2000}};
  manager.addOrUpdateNodeResources(node_address1, resources1);
  manager.addOrUpdateNodeResources(node_address2, resources2);

  EXPECT_TRUE(manager.hasResource(node_address1, "test1.txt"));
  EXPECT_TRUE(manager.hasResource(node_address2, "test2.txt"));

  std::this_thread::sleep_for(std::chrono::seconds(3));
  manager.cleanupStaleNodes();

  EXPECT_FALSE(manager.hasResource(node_address1, "test1.txt"));
  EXPECT_FALSE(manager.hasResource(node_address2, "test2.txt"));
}

TEST_F(RemoteResourceManagerTest, CleanupStaleNodesSkipTest) {
  struct sockaddr_in node_address1 = this->createAddress("192.168.1.1", 8000);
  struct sockaddr_in node_address2 = this->createAddress("192.168.1.2", 8000);
  std::vector<p2p::RemoteResource> resources1 = {{"test1.txt", 1000}};
  std::vector<p2p::RemoteResource> resources2 = {{"test2.txt", 2000}};
  manager.addOrUpdateNodeResources(node_address1, resources1);
  manager.addOrUpdateNodeResources(node_address2, resources2);

  EXPECT_TRUE(manager.hasResource(node_address1, "test1.txt"));
  EXPECT_TRUE(manager.hasResource(node_address2, "test2.txt"));

  std::this_thread::sleep_for(std::chrono::seconds(1));
  manager.cleanupStaleNodes();

  EXPECT_TRUE(manager.hasResource(node_address1, "test1.txt"));
  EXPECT_TRUE(manager.hasResource(node_address2, "test2.txt"));
}
