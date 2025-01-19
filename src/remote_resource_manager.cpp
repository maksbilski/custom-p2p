#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <netinet/in.h>
#include <p2p-resource-sync/remote_resource_manager.hpp>
#include <shared_mutex>
#include <string>
#include <vector>

namespace p2p {

bool RemoteResourceManager::SockAddrCompare::operator()(
    const struct sockaddr_in &a, const struct sockaddr_in &b) const {
  if (a.sin_addr.s_addr != b.sin_addr.s_addr)
    return a.sin_addr.s_addr < b.sin_addr.s_addr;
  return a.sin_port < b.sin_port;
}

std::vector<std::pair<struct sockaddr_in, Resource>>
RemoteResourceManager::getAllResources() const {
  std::shared_lock lock(this->mutex_);
  std::vector<std::pair<struct sockaddr_in, Resource>> resources;
  for (const auto &[node_addr, node_data] : this->nodes_) {
    for (const auto &resource : node_data.resources) {
      resources.emplace_back(node_addr, resource);
    }
  }
  return resources;
};

void RemoteResourceManager::addOrUpdateNodeResources(
    const struct sockaddr_in &node_address,
    const std::vector<Resource> &resources, uint64_t timestamp) {
  std::unique_lock lock(this->mutex_);
  this->nodes_[node_address] =
      RemoteNode{.resources = resources,
                 .lastAnnouncementTime = std::chrono::system_clock::time_point(
                     std::chrono::nanoseconds(timestamp))};
  return;
};

bool RemoteResourceManager::hasResource(
    const struct sockaddr_in &node_address,
    const std::string &resource_name) const {
  std::shared_lock lock(this->mutex_);

  auto node_it = nodes_.find(node_address);
  if (node_it == nodes_.end())
    return false;

  auto resource_it = std::find_if(node_it->second.resources.begin(),
                                  node_it->second.resources.end(),
                                  [&resource_name](const Resource &res) {
                                    return res.name == resource_name;
                                  });
  return resource_it != node_it->second.resources.end();
};

std::vector<struct sockaddr_in> RemoteResourceManager::findNodesWithResource(
    const std::string &resource_name) const {
  std::shared_lock lock(this->mutex_);
  std::vector<struct sockaddr_in> found_nodes;

  for (const auto &[node_addr, node_info] : this->nodes_) {
    auto resource_it =
        std::find_if(node_info.resources.begin(), node_info.resources.end(),
                     [&resource_name](const Resource &res) {
                       return res.name == resource_name;
                     });
    if (resource_it != node_info.resources.end())
      found_nodes.push_back(node_addr);
  }

  return found_nodes;
};

void RemoteResourceManager::cleanupStaleNodes() {
  std::unique_lock lock(this->mutex_);
  auto now = std::chrono::system_clock::now();
  for (auto it = nodes_.begin(); it != nodes_.end();) {
    if (now - it->second.lastAnnouncementTime >= this->cleanup_interval_) {
      it = this->nodes_.erase(it);
    } else {
      ++it;
    }
  }
};
} // namespace p2p
