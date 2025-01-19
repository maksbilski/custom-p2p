#include "p2p-resource-sync/resource_downloader.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <string>

class ResourceDownloaderTest : public ::testing::Test {
protected:
   const std::string test_download_dir = "/tmp/test_downloads";
   
   void SetUp() override {
   }
};

TEST_F(ResourceDownloaderTest, CanBeInstantiated) {
   EXPECT_NO_THROW({
       p2p::ResourceDownloader downloader{test_download_dir};
   });
}
