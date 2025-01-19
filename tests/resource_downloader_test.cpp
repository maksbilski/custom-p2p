#include <p2p-resource-sync/resource_downloader.hpp>
#include <p2p-resource-sync/tcp_server.hpp>
#include <p2p-resource-sync/local_resource_manager.hpp>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <atomic>
#include <cstring>
#include <memory>
#include <vector>
#include <iostream>
#include <future>
#include <chrono>

class ResourceDownloaderTest : public ::testing::Test {
protected:
    ResourceDownloaderTest() 
        : server_port(8081)
        , download_dir("/tmp/test_downloads")
        , test_files_dir("/tmp/test_local_files")
        , test_file_path("/tmp/test_local_files/test.txt")
        , should_stop_(false)
    {
        for (const auto& dir : {download_dir, test_files_dir}) {
            if (!std::filesystem::exists(dir)) {
                std::filesystem::create_directory(dir);
            }
        }

        std::ofstream test_file(test_file_path);
        if (test_file.is_open()) {
            test_file.close();
        } else {
            throw std::runtime_error("Could not create test file");
        }

        manager = std::make_shared<p2p::LocalResourceManager>();
        server = std::make_unique<p2p::TcpServer>(manager, server_port);
        downloader = std::make_unique<p2p::ResourceDownloader>(download_dir);
    }

    ~ResourceDownloaderTest() {
        try {
            if (std::filesystem::exists(test_file_path)) {
                std::filesystem::remove(test_file_path);
            }
            if (std::filesystem::exists(test_files_dir)) {
                std::filesystem::remove(test_files_dir);
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Cleanup error: " << e.what() << std::endl;
        }
    }
void SetUp() override {
    if (!std::filesystem::exists(test_file_path)) {
        throw std::runtime_error("Test file missing: " + test_file_path);
    }
    
    std::ofstream test_file(test_file_path);
    if (!test_file.is_open()) {
        throw std::runtime_error("Could not open test file for writing");
    }

    for (int i = 0; i < 1000; ++i) {
        test_file << "Line " << i << ": This is a test line with some random data.\n";
        test_file << "Some binary data: " << static_cast<char>(i % 256);
        test_file << " More text to test transfer of mixed content.\n";
    }
    
    test_file.close();
    
    manager->addResource("test.txt", test_file_path);
    
    server_thread = std::thread([this]() { 
        server->run();
    });
}
void TearDown() override {
    std::cout << "TearDown: Stopping server thread" << std::endl;
    server->stop();
   
    if (server_thread.joinable()) {
        auto future = std::async(std::launch::async, [this]() {
            server_thread.join();
        });
       
        if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
            std::cerr << "Warning: Server thread join timeout" << std::endl;
        } else {
            std::cout << "Server thread joined successfully" << std::endl;
        }
    }
   
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    try {
        if (std::filesystem::exists(download_dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(download_dir)) {
                try {
                    std::filesystem::remove_all(entry.path());
                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "Error removing " << entry.path() << ": " << e.what() << std::endl;
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error during cleanup: " << e.what() << std::endl;
    }
}

    bool compareFiles(const std::string& file1, const std::string& file2) {
        std::cout << "Comparing files:\n  " << file1 << "\n  " << file2 << std::endl;
        
        std::ifstream f1(file1, std::ios::binary);
        std::ifstream f2(file2, std::ios::binary);
        
        if (!f1 || !f2) {
            std::cout << "Failed to open files for comparison" << std::endl;
            return false;
        }
        
        std::vector<char> buf1(4096);
        std::vector<char> buf2(4096);
        size_t total_bytes_compared = 0;
        
        while (f1 && f2) {
            f1.read(buf1.data(), buf1.size());
            f2.read(buf2.data(), buf2.size());
            
            auto count1 = f1.gcount();
            auto count2 = f2.gcount();
            
            total_bytes_compared += count1;
            
            if (count1 != count2) {
                std::cout << "Files have different sizes at position " << total_bytes_compared << std::endl;
                return false;
            }
            if (memcmp(buf1.data(), buf2.data(), count1) != 0) {
                std::cout << "Content mismatch at position " << total_bytes_compared << std::endl;
                return false;
            }
        }
        
        bool result = f1.eof() && f2.eof();
        std::cout << "File comparison complete. Total bytes compared: " << total_bytes_compared 
                  << " Result: " << (result ? "match" : "mismatch") << std::endl;
        return result;
    }

    std::shared_ptr<p2p::LocalResourceManager> manager;
    std::unique_ptr<p2p::TcpServer> server;
    std::unique_ptr<p2p::ResourceDownloader> downloader;
    std::thread server_thread;
    std::atomic<bool> should_stop_;
    const int server_port;
    const std::string download_dir;
    const std::string test_files_dir;
    const std::string test_file_path;
};


TEST_F(ResourceDownloaderTest, CanDownloadExistingResource) {
    bool result = downloader->downloadResource("127.0.0.1", server_port, "test.txt");
    std::cout << "Download result: " << (result ? "success" : "failure") << std::endl;
    EXPECT_TRUE(result);
    
    std::string downloaded_file = download_dir + "/test.txt";
    std::string original_file = test_file_path;
    
    std::cout << "Checking if downloaded file exists: " << downloaded_file << std::endl;
    bool exists = std::filesystem::exists(downloaded_file);
    std::cout << "File exists: " << (exists ? "yes" : "no") << std::endl;
    EXPECT_TRUE(exists);
    
    std::cout << "Comparing downloaded file with original..." << std::endl;
    EXPECT_TRUE(compareFiles(downloaded_file, original_file));
}

TEST_F(ResourceDownloaderTest, ConcurrentDownloadsStressTest) {
    const int NUM_CLIENTS = 25;
    std::vector<std::unique_ptr<p2p::ResourceDownloader>> downloaders;
    std::vector<std::future<bool>> download_futures;
    
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        std::string client_dir = download_dir + "/client_" + std::to_string(i);
        std::filesystem::create_directory(client_dir);
        downloaders.push_back(std::make_unique<p2p::ResourceDownloader>(client_dir));
    }
    
    std::cout << "Starting " << NUM_CLIENTS << " concurrent downloads..." << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        download_futures.push_back(
            std::async(std::launch::async, 
                [&downloaders, i, this]() {
                    return downloaders[i]->downloadResource("127.0.0.1", server_port, "test.txt");
                }
            )
        );
    }
    
    std::vector<bool> results;
    for (auto& future : download_futures) {
        results.push_back(future.get());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "All downloads completed in " << duration.count() << "ms" << std::endl;
    
    EXPECT_EQ(std::count(results.begin(), results.end(), true), NUM_CLIENTS);
    
    int successful_comparisons = 0;
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        std::string client_file = download_dir + "/client_" + std::to_string(i) + "/test.txt";
        std::cout << "Verifying download for client " << i << "..." << std::endl;
        
        EXPECT_TRUE(std::filesystem::exists(client_file));
        
        if (compareFiles(client_file, test_file_path)) {
            successful_comparisons++;
        }
    }
    
    EXPECT_EQ(successful_comparisons, NUM_CLIENTS);
    
    std::cout << "Concurrent download test completed. "
              << successful_comparisons << "/" << NUM_CLIENTS 
              << " files verified successfully." << std::endl;
}
