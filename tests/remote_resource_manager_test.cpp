#include "p2p-resource-sync/remote_resource_manager.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <ctime>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>

class RemoteResourceManagerTest : public ::testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(RemoteResourceManagerTest, CanBeInstantiated) {
  EXPECT_NO_THROW(
      { p2p::RemoteResourceManager manager(std::chrono::seconds(60)); });
}

TEST_F(RemoteResourceManagerTest, CannotBeCopied) {
  p2p::RemoteResourceManager manager(std::chrono::seconds(60));

  EXPECT_FALSE(std::is_copy_constructible_v<p2p::RemoteResourceManager>);
  EXPECT_FALSE(std::is_copy_assignable_v<p2p::RemoteResourceManager>);
}

TEST_F(RemoteResourceManagerTest, EmptyManagerHasNoResources) {
  p2p::RemoteResourceManager manager(std::chrono::seconds(60));

  EXPECT_TRUE(manager.getAllResources().empty());
}

TEST_F(RemoteResourceManagerTest, CanAddSingleResourceToNode) {
  p2p::RemoteResourceManager manager(std::chrono::seconds(60));
  struct sockaddr_in node_address1;
  memset(&node_address1, 0, sizeof(node_address1));
  node_address1.sin_family = AF_INET;
  node_address1.sin_port = htons(8080);
  node_address1.sin_addr.s_addr = inet_addr("127.0.0.1");
  p2p::RemoteResource resource{.name = "test_resource", .size = 100};

  manager.addOrUpdateNodeResources(node_address1, {resource});

  EXPECT_FALSE(manager.getAllResources().empty());
}

TEST_F(RemoteResourceManagerTest, CanAddMultipleResourcesToNode) {
  p2p::RemoteResourceManager manager(std::chrono::seconds(60));
  struct sockaddr_in node_address1;
  memset(&node_address1, 0, sizeof(node_address1));
  node_address1.sin_family = AF_INET;
  node_address1.sin_port = htons(8080);
  node_address1.sin_addr.s_addr = inet_addr("127.0.0.1");
  p2p::RemoteResource resource1{.name = "test_resource1", .size = 100};
  p2p::RemoteResource resource2{.name = "test_resource2", .size = 100};

  manager.addOrUpdateNodeResources(node_address1, {resource1, resource2});

  EXPECT_TRUE(manager.getAllResources().size() == 2);
}

TEST_F(RemoteResourceManagerTest, CanAddMultipleNodesWithResources) {
  p2p::RemoteResourceManager manager(std::chrono::seconds(60));
  struct sockaddr_in node_address1;
  memset(&node_address1, 0, sizeof(node_address1));
  node_address1.sin_family = AF_INET;
  node_address1.sin_port = htons(8080);
  node_address1.sin_addr.s_addr = inet_addr("127.0.0.1");
  struct sockaddr_in node_address2;
  memset(&node_address2, 0, sizeof(node_address2));
  node_address2.sin_family = AF_INET;
  node_address2.sin_port = htons(8080);
  node_address2.sin_addr.s_addr = inet_addr("127.0.0.2");
  p2p::RemoteResource resource1{.name = "test_resource1", .size = 100};
  p2p::RemoteResource resource2{.name = "test_resource2", .size = 100};

  manager.addOrUpdateNodeResources(node_address1, {resource1, resource2});
  manager.addOrUpdateNodeResources(node_address2, {resource1, resource2});

  EXPECT_TRUE(manager.getAllResources().size() == 4);
}

TEST_F(RemoteResourceManagerTest, NodeHasResource) {
  p2p::RemoteResourceManager manager(std::chrono::seconds(60));
  struct sockaddr_in node_address1;
  memset(&node_address1, 0, sizeof(node_address1));
  node_address1.sin_family = AF_INET;
  node_address1.sin_port = htons(8080);
  node_address1.sin_addr.s_addr = inet_addr("127.0.0.1");
  p2p::RemoteResource resource{.name = "test_resource", .size = 100};

  manager.addOrUpdateNodeResources(node_address1, {resource});

  EXPECT_TRUE(manager.hasResource(node_address1, "test_resource"));
}

TEST_F(RemoteResourceManagerTest, NodeHasNoResource) {
  p2p::RemoteResourceManager manager(std::chrono::seconds(60));
  struct sockaddr_in node_address1;
  memset(&node_address1, 0, sizeof(node_address1));
  node_address1.sin_family = AF_INET;
  node_address1.sin_port = htons(8080);
  node_address1.sin_addr.s_addr = inet_addr("127.0.0.1");
  p2p::RemoteResource resource{.name = "test_resource", .size = 100};

  manager.addOrUpdateNodeResources(node_address1, {resource});

  EXPECT_FALSE(manager.hasResource(node_address1, "test_resource2"));
}

TEST_F(RemoteResourceManagerTest, FindNodeWithResource) {
  p2p::RemoteResourceManager manager(std::chrono::seconds(60));
  struct sockaddr_in node_address1;
  memset(&node_address1, 0, sizeof(node_address1));
  node_address1.sin_family = AF_INET;
  node_address1.sin_port = htons(8080);
  node_address1.sin_addr.s_addr = inet_addr("127.0.0.1");
  p2p::RemoteResource resource{.name = "test_resource", .size = 100};

  manager.addOrUpdateNodeResources(node_address1, {resource});

  std::vector<struct sockaddr_in> found_nodes =
      manager.findNodesWithResource("test_resource");

  EXPECT_FALSE(found_nodes.empty());
}

TEST_F(RemoteResourceManagerTest, FindMultipleNodesWithResource) {
  p2p::RemoteResourceManager manager(std::chrono::seconds(60));
  struct sockaddr_in node_address1;
  memset(&node_address1, 0, sizeof(node_address1));
  node_address1.sin_family = AF_INET;
  node_address1.sin_port = htons(8080);
  node_address1.sin_addr.s_addr = inet_addr("127.0.0.1");
  struct sockaddr_in node_address2;
  memset(&node_address2, 0, sizeof(node_address2));
  node_address2.sin_family = AF_INET;
  node_address2.sin_port = htons(8080);
  node_address2.sin_addr.s_addr = inet_addr("127.0.0.2");
  p2p::RemoteResource resource{.name = "test_resource", .size = 100};

  manager.addOrUpdateNodeResources(node_address1, {resource});
  manager.addOrUpdateNodeResources(node_address2, {resource});

  std::vector<struct sockaddr_in> found_nodes =
      manager.findNodesWithResource("test_resource");

  EXPECT_TRUE(found_nodes.size() == 2);
}

TEST_F(RemoteResourceManagerTest, NoNodesWithResourceFound) {
  p2p::RemoteResourceManager manager(std::chrono::seconds(60));
  struct sockaddr_in node_address1;
  memset(&node_address1, 0, sizeof(node_address1));
  node_address1.sin_family = AF_INET;
  node_address1.sin_port = htons(8080);
  node_address1.sin_addr.s_addr = inet_addr("127.0.0.1");
  struct sockaddr_in node_address2;
  memset(&node_address2, 0, sizeof(node_address2));
  node_address2.sin_family = AF_INET;
  node_address2.sin_port = htons(8080);
  node_address2.sin_addr.s_addr = inet_addr("127.0.0.2");
  p2p::RemoteResource resource{.name = "test_resource", .size = 100};

  manager.addOrUpdateNodeResources(node_address1, {resource});
  manager.addOrUpdateNodeResources(node_address2, {resource});

  std::vector<struct sockaddr_in> found_nodes =
      manager.findNodesWithResource("test_resource1");

  EXPECT_TRUE(found_nodes.empty());
}
