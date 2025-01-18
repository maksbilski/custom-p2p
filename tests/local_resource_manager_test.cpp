#include <gtest/gtest.h>
#include "p2p-resource-sync/local_resource_manager.hpp"

class LocalResourceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(LocalResourceManagerTest, CanBeInstantiated) {
    EXPECT_NO_THROW({
        p2p::LocalResourceManager manager;
    });
}

