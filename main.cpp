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

class Application {
public:
  Application(uint16_t sender_port, uint16_t broadcast_port)
      : local_resource_manager_(std::make_shared<p2p::LocalResourceManager>()),
        remote_resource_manager_(std::make_shared<p2p::RemoteResourceManager>(
            std::chrono::seconds(150))),
        broadcaster_(local_resource_manager_, sender_port, broadcast_port),
        receiver_(remote_resource_manager_, broadcast_port),
        tcp_server_(local_resource_manager_), downloader_("downloads/") {

    // Start components in separate threads
    broadcaster_thread_ = std::jthread([this]() { broadcaster_.run(); });
    receiver_thread_ = std::jthread([this]() { receiver_.run(); });
    tcp_server_thread_ = std::jthread([this]() { tcp_server_.run(); });
  }

  ~Application() { stop(); }

  void stop() {
    broadcaster_.stop();
    receiver_.stop();
    if (broadcaster_thread_.joinable())
      broadcaster_thread_.join();
    if (receiver_thread_.joinable())
      receiver_thread_.join();
    if (tcp_server_thread_.joinable())
      tcp_server_thread_.join();
  }

  void run() {
    while (running_) {
      displayMenu();
      std::string input;
      std::getline(std::cin, input);

      try {
        handleUserInput(input);
      } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
      }
    }
  }

private:
  void displayMenu() {
    std::cout << "\nP2P File Sharing System\n"
              << "1. List local resources\n"
              << "2. List network resources\n"
              << "3. Add local resource\n"
              << "4. Download resource\n"
              << "5. Exit\n"
              << "Enter command: ";
  }

  void handleUserInput(const std::string &input) {
    int choice = std::stoi(input);
    switch (choice) {
    case 1:
      listLocalResources();
      break;
    case 2:
      listNetworkResources();
      break;
    case 3:
      addLocalResource();
      break;
    case 4:
      downloadResource();
      break;
    case 5:
      running_ = false;
      break;
    default:
      std::cout << "Invalid command\n";
    }
  }

  void listLocalResources() {
    auto resources = local_resource_manager_->getAllResources();
    std::cout << "\nLocal resources:\n";
    for (const auto &resource : resources) {
      std::cout << "- " << resource.first << " (" << resource.second.size
                << " bytes)\n";
    }
  }

  void listNetworkResources() {
    auto resources = remote_resource_manager_->getAllResources();
    std::cout << "\nNetwork resources:\n";
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
      local_resource_manager_->addResource(path, name);
      std::cout << "Resource added successfully\n";
    } catch (const std::exception &e) {
      std::cout << "Failed to add resource: " << e.what() << "\n";
    }
  }

  void downloadResource() {
    std::cout << "Enter resource name: ";
    std::string name;
    std::getline(std::cin, name);

    auto nodes = remote_resource_manager_->findNodesWithResource(name);
    if (nodes.empty()) {
      std::cout << "Resource not found in network\n";
      return;
    }

    // Use first available node
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(nodes[0].sin_addr), ip, INET_ADDRSTRLEN);
    int port = ntohs(nodes[0].sin_port);

    try {
      bool success = downloader_.downloadResource(ip, port, name);
      if (success) {
        std::cout << "Download completed successfully\n";
        local_resource_manager_->addResource("downloads/" + name, name);
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
  std::atomic<bool> running_{true};
};

std::atomic<bool> shutdown_requested{false};

void signalHandler(int) { shutdown_requested = true; }

int main(int argc, char *argv[]) {
  try {
    if (argc != 3) {
      std::cerr << "Usage: " << argv[0] << " <sender_port> <broadcast_port\n";
      return 1;
    }

    uint16_t sender_port = static_cast<uint16_t>(std::stoi(argv[1]));
    uint16_t broadcast_port = static_cast<uint16_t>(std::stoi(argv[2]));

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    Application app(sender_port, broadcast_port);

    while (!shutdown_requested) {
      app.run();
    }

    std::cout << "\nShutting down...\n";
    app.stop();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
}
