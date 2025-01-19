#include "p2p-resource-sync/local_resource_manager.hpp"
#include <gtest/gtest.h>

class LocalResourceManagerTest : public ::testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(LocalResourceManagerTest, CanBeInstantiated) {
  EXPECT_NO_THROW({ p2p::LocalResourceManager manager; });
}

TEST_F(LocalResourceManagerTest, CannotBeCopied) {
  p2p::LocalResourceManager manager1;

  EXPECT_FALSE(std::is_copy_constructible_v<p2p::LocalResourceManager>);
  EXPECT_FALSE(std::is_copy_assignable_v<p2p::LocalResourceManager>);
}

TEST_F(LocalResourceManagerTest, AddResource) {
  p2p::LocalResourceManager manager1;
  manager1.addResource(std::string("some_name"), std::string("some_path"));
}
