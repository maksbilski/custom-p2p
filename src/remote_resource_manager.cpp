#include <algorithm>
#include <chrono>
#include <netinet/in.h>
#include <p2p-resource-sync/remote_resource_manager.hpp>
#include <string>
#include <vector>

namespace p2p {

bool RemoteResourceManager::SockAddrCompare::operator()(
    const struct sockaddr_in &a, const struct sockaddr_in &b) const {
  if (a.sin_addr.s_addr != b.sin_addr.s_addr)
    return a.sin_addr.s_addr < b.sin_addr.s_addr;
  return a.sin_port < b.sin_port;
}

std::vector<std::pair<struct sockaddr_in, RemoteResource>>
RemoteResourceManager::getAllResources() const {
  std::vector<std::pair<struct sockaddr_in, RemoteResource>> resources;
  for (const auto &[node_addr, node_data] : this->nodes) {
    for (const auto &resource : node_data.resources) {
      resources.emplace_back(node_addr, resource);
    }
  }
  return resources;
};

void RemoteResourceManager::addOrUpdateNodeResources(
    const struct sockaddr_in &node_address,
    const std::vector<RemoteResource> &resources) {
  this->nodes[node_address] =
      RemoteNode{.resources = resources,
                 .last_announcement = std::chrono::system_clock::now()};
  return;
};

bool RemoteResourceManager::hasResource(
    const struct sockaddr_in &node_address,
    const std::string &resource_name) const {
  auto node_it = nodes.find(node_address);
  if (node_it == nodes.end())
    return false;

  auto resource_it = std::find_if(node_it->second.resources.begin(),
                                  node_it->second.resources.end(),
                                  [&resource_name](const RemoteResource &res) {
                                    return res.name == resource_name;
                                  });
  return resource_it != node_it->second.resources.end();
};

std::vector<struct sockaddr_in> RemoteResourceManager::findNodesWithResource(
    const std::string &resource_name) const {
  std::vector<struct sockaddr_in> found_nodes;

  for (const auto &[node_addr, node_info] : this->nodes) {
    auto resource_it =
        std::find_if(node_info.resources.begin(), node_info.resources.end(),
                     [&resource_name](const RemoteResource &res) {
                       return res.name == resource_name;
                     });
    if (resource_it != node_info.resources.end())
      found_nodes.push_back(node_addr);
  }

  return found_nodes;
};

} // namespace p2p
