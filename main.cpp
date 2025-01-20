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
#include <iostream>
#include <memory>
#include <string>
#include <thread>

std::atomic<bool> shutdown_requested{false};

class Application {
public:
  Application(uint32_t node_id, uint16_t sender_port, uint16_t broadcast_port,
              uint16_t tcp_port)
      : local_resource_manager_(std::make_shared<p2p::LocalResourceManager>()),
        remote_resource_manager_(std::make_shared<p2p::RemoteResourceManager>(
            std::chrono::seconds(90))),
        broadcaster_(local_resource_manager_, node_id, sender_port,
                     broadcast_port),
        receiver_(remote_resource_manager_, node_id, broadcast_port),
        tcp_server_(local_resource_manager_, tcp_port),
        downloader_("/home/mrozek/Downloads/"), tcp_port_(tcp_port) {

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

      try {
        this->handleUserInput_(input);
      } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
      }
    }
  }

private:
  void displayMenu_() {
    std::cout << "\nP2P File Sharing System\n"
              << "1. List local resources\n"
              << "2. List network resources\n"
              << "3. Add local resource\n"
              << "4. Download resource\n"
              << "5. Exit\n"
              << "Enter command: ";
  }

  void handleUserInput_(const std::string &input) {
    int choice = std::stoi(input);
    switch (choice) {
    case 1:
      this->listLocalResources();
      break;
    case 2:
      this->listRemoteResources();
      break;
    case 3:
      this->addLocalResource();
      break;
    case 4:
      this->downloadResource();
      break;
    case 5:
      shutdown_requested = true;
      break;
    default:
      std::cout << "Invalid command\n";
    }
  }

  void listLocalResources() {
    auto resources = this->local_resource_manager_->getAllResources();
    std::cout << "\nLocal resources:\n";
    for (const auto &resource : resources) {
      std::cout << "- " << resource.first << " (" << resource.second.size
                << " bytes)\n";
    }
  }

  void listRemoteResources() {
    auto resources = this->remote_resource_manager_->getAllResources();
    std::cout << "\nRemote resources:\n";
    for (const auto &[addr, resource] : resources) {
      char ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
      std::cout << "- " << resource.name << " (" << resource.size
                << " bytes) at " << ip << ":" << ntohs(addr.sin_port) << "\n";
    }
  }

  void addLocalResource() {
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

  void downloadResource() {
    std::cout << "Enter resource name: ";
    std::string name;
    std::getline(std::cin, name);

    auto nodes = this->remote_resource_manager_->findNodesWithResource(name);
    if (nodes.empty()) {
      std::cout << "Resource not found \n";
      return;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(nodes[0].sin_addr), ip, INET_ADDRSTRLEN);

    try {
      bool success =
          this->downloader_.downloadResource(ip, this->tcp_port_, name);
      if (success) {
        std::cout << "Download completed successfully\n";
        this->local_resource_manager_->addResource(
            name, "/home/mrozek/Downloads/" + name);
      } else {
        std::cout << "Download failed\n";
      }
    } catch (const std::exception &e) {
      std::cout << "Download failed: " << e.what() << "\n";
    }
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
    if (argc != 5) {
      std::cerr << "Usage: " << argv[0]
                << " <node_id> <udp_port> <broadcast_port> <tcp_port>\n";
      return 1;
    }

    uint32_t node_id = static_cast<uint16_t>(std::stoi(argv[1]));
    uint16_t sender_port = static_cast<uint16_t>(std::stoi(argv[2]));
    uint16_t broadcast_port = static_cast<uint16_t>(std::stoi(argv[3]));
    uint16_t tcp_port = static_cast<uint16_t>(std::stoi(argv[4]));

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    Application app(node_id, sender_port, broadcast_port, tcp_port);

    app.run();

    std::cout << "\nShutting down...\n";
    app.stop();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
}
