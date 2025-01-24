#include "p2p-resource-sync/announcement_broadcaster.hpp"
#include "p2p-resource-sync/announcement_receiver.hpp"
#include "p2p-resource-sync/local_resource_manager.hpp"
#include "p2p-resource-sync/remote_resource_manager.hpp"
#include "p2p-resource-sync/resource_downloader.hpp"
#include "p2p-resource-sync/tcp_server.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <thread>

std::atomic<bool> shutdown_requested{false};

class Application {
public:
  Application(uint32_t node_id, uint16_t sender_port, uint16_t broadcast_port,
              uint16_t tcp_port, bool simulate_drops)
      : local_resource_manager_(std::make_shared<p2p::LocalResourceManager>()),
        remote_resource_manager_(std::make_shared<p2p::RemoteResourceManager>(
            std::chrono::seconds(60))),
        broadcaster_(local_resource_manager_, node_id, sender_port,
                     broadcast_port, std::chrono::seconds(5)),
        receiver_(remote_resource_manager_, node_id, broadcast_port),
        tcp_server_(local_resource_manager_, tcp_port, 10, simulate_drops),
        downloader_("downloads/"), tcp_port_(tcp_port) {

    this->broadcaster_thread_ = std::jthread([this]() { broadcaster_.run(); });
    this->receiver_thread_ = std::jthread([this]() { receiver_.run(); });
    this->tcp_server_thread_ = std::jthread([this]() { tcp_server_.run(); });
    this->cleanup_thread_ = std::jthread([this]() {
      while (!shutdown_requested) {
        remote_resource_manager_->cleanupStaleNodes();
        std::this_thread::sleep_for(std::chrono::seconds(10));
      }
    });
  }

  ~Application() { stop(); }

  void stop() {
    this->broadcaster_.stop();
    this->receiver_.stop();
    this->tcp_server_.stop();
    if (this->broadcaster_thread_.joinable())
      this->broadcaster_thread_.join();
    if (this->receiver_thread_.joinable())
      this->receiver_thread_.join();
    if (this->tcp_server_thread_.joinable())
      this->tcp_server_thread_.join();
    if (this->cleanup_thread_.joinable())
      this->cleanup_thread_.join();
  }

  void run() {
    while (!shutdown_requested) {
      this->displayMenu_();
      std::string input;
      std::getline(std::cin, input);
      this->handleUserInput_(input);
    }
  }

private:
  void displayMenu_() {
    std::cout << "\nP2P File Sharing System\n"
              << "1. List local resources\n"
              << "2. List network resources\n"
              << "3. Add local resource\n"
              << "4. Remove local resource\n"
              << "5. Download resource\n"
              << "6. Exit\n"
              << "Enter command: ";
  }

  void handleUserInput_(const std::string &input) {
    int choice = std::stoi(input);
    switch (choice) {
    case 1:
      this->listLocalResources_();
      break;
    case 2:
      this->listRemoteResources_();
      break;
    case 3:
      this->addLocalResource_();
      break;
    case 4:
      this->removeLocalResource_();
      break;
    case 5:
      this->downloadRemoteResource_();
      break;
    case 6:
      shutdown_requested = true;
      break;
    default:
      std::cout << "Invalid command\n";
    }
  }

  void listLocalResources_() {
    auto resources = this->local_resource_manager_->getAllResources();
    std::cout << "\nLocal resources:\n";
    for (const auto &resource : resources) {
      std::cout << "- " << resource.first << " (" << resource.second.size
                << " bytes)\n";
    }
  }

  void listRemoteResources_() {
    auto resources = this->remote_resource_manager_->getAllResources();
    std::cout << "\nRemote resources:\n";
    for (const auto &[addr, resource] : resources) {
      char ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
      std::cout << "- " << resource.name << " (" << resource.size
                << " bytes) at " << ip << "\n";
    }
  }

  void addLocalResource_() {
    std::cout << "Enter resource path: ";
    std::string path;
    std::getline(std::cin, path);

    std::cout << "Enter resource name: ";
    std::string name;
    std::getline(std::cin, name);
    try {
      this->local_resource_manager_->addResource(name, path);
      std::cout << "Resource added successfully\n";
    } catch (const std::exception &e) {
      std::cout << "Failed to add resource: " << e.what() << "\n";
    }
  }

  void removeLocalResource_() {
    std::cout << "Enter resource name: ";
    std::string name;
    std::getline(std::cin, name);

    if (!this->local_resource_manager_->removeResource(name)) {
      std::cout << "Resource not found: " << name << std::endl;
      return;
    }
    std::cout << "Resource removed successfully" << std::endl;
  }

  void downloadRemoteResource_() {
    std::cout << "Enter resource name: ";
    std::string name;
    std::getline(std::cin, name);
    uint64_t offset = 0;

    while (true) {
      auto nodes = this->remote_resource_manager_->findNodesWithResource(name);
      if (nodes.empty()) {
        std::cout << "Resource not found: " << name << std::endl;
        return;
      }

      size_t choice = this->chooseNodeToDownload_(nodes);

      char chosen_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(nodes[choice - 1].sin_addr), chosen_ip,
                INET_ADDRSTRLEN);

      try {
        auto [received, total_size] = this->downloader_.downloadResource(
            chosen_ip, this->tcp_port_, offset, name);

        if (total_size == 0) {
          std::cout << "Download failed, resource not found" << std::endl;
        } else if (received == total_size) {
          std::cout << "Download completed successfully" << std::endl;
          this->local_resource_manager_->addResource(name, "downloads/" + name);
          return;
        } else if (received > 0) {
          std::cout << "Download incomplete: " << received << "/" << total_size
                    << " bytes" << std::endl;
          offset = received;
          std::cout << "Would you like to try again from offset " << offset
                    << "? (y/n): ";
          std::string retry;
          std::getline(std::cin, retry);
          if (retry != "y" && retry != "Y") {
            return;
          }
        } else {
          std::cout << "Download failed, no data received" << std::endl;
          std::cout << "Would you like to try again? (y/n): ";
          std::string retry;
          std::getline(std::cin, retry);
          if (retry != "y" && retry != "Y") {
            return;
          }
        }
      } catch (const std::exception &e) {
        std::cout << "Download failed: " << e.what() << "\n";
        return;
      }

      std::cout << std::endl << "Retrying download..." << std::endl;
    }
  }

  size_t chooseNodeToDownload_(const std::vector<struct sockaddr_in> &nodes) {
    size_t choice;
    std::cout << "Found " << nodes.size() << " nodes with resource"
              << std::endl;
    if (nodes.size() > 1) {
      for (size_t i = 0; i < nodes.size(); i++) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(nodes[i].sin_addr), ip, INET_ADDRSTRLEN);
        std::cout << i + 1 << " - " << ip << std::endl;
      }

      while (true) {
        std::cout << "Choose node number (1-" << nodes.size() << "): ";
        std::string input;
        std::getline(std::cin, input);

        try {
          choice = std::stoul(input);
          if (choice >= 1 && choice <= nodes.size()) {
            break;
          }
          std::cout << "Number must be from range (1-" << nodes.size() << ")"
                    << std::endl;
        } catch (const std::exception &) {
          std::cout << "Invalid input" << std::endl;
        }
      }
    } else {
      choice = 1;
    }
    return choice;
  }

  std::shared_ptr<p2p::LocalResourceManager> local_resource_manager_;
  std::shared_ptr<p2p::RemoteResourceManager> remote_resource_manager_;
  p2p::AnnouncementBroadcaster broadcaster_;
  p2p::AnnouncementReceiver receiver_;
  p2p::TcpServer tcp_server_;
  p2p::ResourceDownloader downloader_;
  std::jthread broadcaster_thread_;
  std::jthread receiver_thread_;
  std::jthread tcp_server_thread_;
  std::jthread cleanup_thread_;
  uint16_t tcp_port_;
};

void signalHandler(int) { shutdown_requested = true; }

int main(int argc, char *argv[]) {
  try {
    if (argc != 5 && argc != 6) {
      std::cerr << "Usage: " << argv[0]
                << " <node_id> <udp_port> <broadcast_port> <tcp_port> [simulate_drops]\n";
      return 1;
    }
    if (!std::filesystem::exists("downloads")) {
      std::filesystem::create_directories("downloads");
    }
    uint32_t node_id = static_cast<uint16_t>(std::stoi(argv[1]));
    uint16_t sender_port = static_cast<uint16_t>(std::stoi(argv[2]));
    uint16_t broadcast_port = static_cast<uint16_t>(std::stoi(argv[3]));
    uint16_t tcp_port = static_cast<uint16_t>(std::stoi(argv[4]));
    bool simulate_drops = argc == 6 && std::stoi(argv[5]) != 0;
    std::cout << simulate_drops;
    
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    Application app(node_id, sender_port, broadcast_port, tcp_port, simulate_drops);
    app.run();
    std::cout << "\nShutting down...\n";
    app.stop();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
}
