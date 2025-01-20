#include "p2p-resource-sync/local_resource_manager.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace {
class LocalResourceManagerThreadTest : public ::testing::Test {
protected:
  static constexpr int NUM_THREADS = 4;
  static constexpr int OPERATIONS_PER_THREAD = 1000;

  void SetUp() override {
    for (int i = 0; i < NUM_THREADS; ++i) {
      temp_paths_.push_back(createTempFile("test_file_" + std::to_string(i)));
    }
  }

  void TearDown() override {
    for (const auto &path : temp_paths_) {
      removeTempFile(path);
    }
  }

  std::string createTempFile(const std::string &name) {
    auto temp_path = std::filesystem::temp_directory_path() / name;
    std::ofstream file(temp_path);
    file << "test content";
    file.close();
    return temp_path.string();
  }

  void removeTempFile(const std::string &path) {
    std::filesystem::remove(path);
  }

  std::vector<std::string> temp_paths_;
};

TEST_F(LocalResourceManagerThreadTest, ConcurrentReads) {
  p2p::LocalResourceManager manager;

  for (int i = 0; i < NUM_THREADS; ++i) {
    ASSERT_TRUE(
        manager.addResource("resource_" + std::to_string(i), temp_paths_[i]));
  }

  std::vector<std::thread> threads;
  std::atomic<int> successful_reads{0};

  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back([&manager, &successful_reads, i]() {
      for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
        auto resources = manager.getAllResources();
        auto resource_path =
            manager.getResourcePath("resource_" + std::to_string(i));
        if (resource_path.has_value()) {
          successful_reads++;
        }
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  EXPECT_EQ(successful_reads, NUM_THREADS * OPERATIONS_PER_THREAD);
}

TEST_F(LocalResourceManagerThreadTest, ConcurrentWrites) {
  p2p::LocalResourceManager manager;
  std::vector<std::thread> threads;
  std::atomic<int> successful_adds{0};
  std::atomic<int> successful_removes{0};

  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back(
        [&manager, &successful_adds, &successful_removes, i, this]() {
          for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
            std::string resource_name =
                "resource_" + std::to_string(i) + "_" + std::to_string(j);

            if (manager.addResource(resource_name, temp_paths_[i])) {
              successful_adds++;

              if (manager.removeResource(resource_name)) {
                successful_removes++;
              }
            }
          }
        });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  EXPECT_EQ(successful_adds, successful_removes);
  EXPECT_TRUE(manager.getAllResources().empty());
}

TEST_F(LocalResourceManagerThreadTest, ConcurrentReadsAndWrites) {
  p2p::LocalResourceManager manager;
  std::vector<std::thread> threads;
  std::atomic<int> successful_operations{0};

  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back([&manager, &successful_operations, i, this]() {
      for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
        std::string resource_name =
            "resource_" + std::to_string(i) + "_" + std::to_string(j);

        switch (j % 3) {
        case 0:
          if (manager.addResource(resource_name, temp_paths_[i])) {
            successful_operations++;
          }
          break;
        case 2:
          manager.getResourceInfo(resource_name);
          successful_operations++;
          break;
        case 3:
          manager.getAllResources();
          successful_operations++;
          break;
        }
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  EXPECT_EQ(successful_operations, NUM_THREADS * OPERATIONS_PER_THREAD);
}

TEST_F(LocalResourceManagerThreadTest, ConcurrentAccessToSameResource) {
  p2p::LocalResourceManager manager;
  std::vector<std::thread> threads;
  std::atomic<int> successful_operations{0};
  const std::string shared_resource_name = "shared_resource";

  ASSERT_TRUE(manager.addResource(shared_resource_name, temp_paths_[0]));

  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back(
        [&manager, &successful_operations, &shared_resource_name]() {
          for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
            switch (j % 3) {
            case 0:
              manager.getResourceInfo(shared_resource_name);
              successful_operations++;
              break;
            case 1:
              manager.getResourcePath(shared_resource_name);
              successful_operations++;
              break;
            case 2:
              manager.getAllResources();
              successful_operations++;
              break;
            }
          }
        });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  EXPECT_EQ(successful_operations, NUM_THREADS * OPERATIONS_PER_THREAD);
}

} // namespace
