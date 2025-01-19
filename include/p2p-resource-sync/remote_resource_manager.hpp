#pragma once

#include <chrono>
#include <cstddef>
#include <ctime>
#include <map>
#include <netinet/in.h>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

namespace p2p {

typedef struct {
  std::string name;
  std::size_t size;
} RemoteResource;

typedef struct {
  std::vector<RemoteResource> resources;
  std::chrono::system_clock::time_point lastAnnouncementTime;
} RemoteNode;

class RemoteResourceManager {
public:
  RemoteResourceManager(std::chrono::seconds interval)
      : cleanup_interval_(interval) {};
  ~RemoteResourceManager() = default;

  RemoteResourceManager(const RemoteResourceManager &) = delete;
  RemoteResourceManager &operator=(const RemoteResourceManager &) = delete;

  std::vector<std::pair<struct sockaddr_in, RemoteResource>>
  getAllResources() const;

  void addOrUpdateNodeResources(const struct sockaddr_in &node_address,
                                const std::vector<RemoteResource> &resources);

  bool hasResource(const struct sockaddr_in &node_addess,
                   const std::string &resource_name) const;

  std::vector<struct sockaddr_in>
  findNodesWithResource(const std::string &resource_name) const;

  void cleanupStaleNodes();

private:
  struct SockAddrCompare {
    bool operator()(const struct sockaddr_in &a,
                    const struct sockaddr_in &b) const;
  };
  mutable std::shared_mutex mutex_;
  std::map<struct sockaddr_in, RemoteNode, SockAddrCompare> nodes_;
  const std::chrono::seconds cleanup_interval_;
};

}
