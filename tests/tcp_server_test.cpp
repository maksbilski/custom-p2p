#include "p2p-resource-sync/tcp_server.hpp"
#include "p2p-resource-sync/local_resource_manager.hpp"
#include <gtest/gtest.h>
#include <memory>

class TcpServerTest : public ::testing::Test {
protected:
    std::shared_ptr<p2p::LocalResourceManager> resource_manager;
    
    void SetUp() override {
        resource_manager = std::make_shared<p2p::LocalResourceManager>();
    }
};

TEST_F(TcpServerTest, CanBeInstantiated) {
    EXPECT_NO_THROW({
        p2p::TcpServer server(resource_manager);
    });
}
