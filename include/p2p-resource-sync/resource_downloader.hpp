#pragma once
#include "protocol.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <netinet/in.h>
#include <string>

namespace p2p {

/**
 * @brief Structure representing download progress information
 *
 * Provides information about the current state of a resource download,
 * including total size, downloaded bytes, and transfer speed.
 */
struct DownloadProgress {
  std::string resourceName;
  uint64_t totalSize;
  uint64_t downloadedBytes;
  double speedMBps;
  bool completed;
};
/**
 * @brief Class responsible for downloading resources from remote peers
 *
 * Implements TCP client functionality for requesting and downloading resources
 * from remote nodes. Supports resuming interrupted downloads and provides
 * progress tracking.
 */
class ResourceDownloader {
public:
  using ProgressCallback = std::function<void(const DownloadProgress &)>;

  /**
   * @brief Constructor
   * @param download_dir Directory where downloaded resources will be saved
   * @param connect_timeout_ms Connection timeout in milliseconds
   */
  explicit ResourceDownloader(const std::string &download_dir,
                              uint32_t socket_timeout_ms = 60000);

  ~ResourceDownloader() = default;

  // Delete copy operations
  ResourceDownloader(const ResourceDownloader &) = delete;
  ResourceDownloader &operator=(const ResourceDownloader &) = delete;
  /**
   * @brief Downloads resource from specified peer
   *
   * Establishes TCP connection with peer and downloads requested resource.
   * Supports automatic resume of interrupted downloads.
   *
   * @param peer_addr Address of peer hosting the resource
   * @param peer_port Port number of peer's TCP server
   * @param resource_name Name of resource to download
   * @return true if download completed successfully, false otherwise
   * @throws ResourceError if connection or transfer fails
   */
  std::pair<uint64_t, uint64_t>
  downloadResource(const std::string &peer_addr, int peer_port, uint64_t offset,
                   const std::string &resource_name);

private:
  const std::string download_dir_;
  const uint32_t socket_timeout_ms_;
  int initialize_socket_(const std::string &host, int port) const;
  void send_resource_request_(const int sock, const uint64_t offset,
                              const std::string &resource_name) const;
  std::unique_ptr<ResourceRequest>
  create_resource_request_(const uint64_t offset,
                           const std::string &resource_name) const;
  std::pair<bool, uint64_t> receive_initial_response_(int sock) const;
  uint64_t receive_file_(int sock, uint64_t offset,
                         const std::string &resource_name,
                         uint64_t file_size) const;
};

} // namespace p2p
