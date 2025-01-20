#pragma once
#include "local_resource_manager.hpp"
#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>

namespace p2p {
/**
 * @brief TCP server handling resource download requests
 *
 * Provides TCP server functionality for handling incoming resource download
 * requests. Implements resource sending according to the protocol
 * specification. Handles resource requests with offsets for resuming
 * interrupted transfers.
 */
class TcpServer {
public:
  /**
   * @brief Constructs the TCP server with specified resource manager and
   * default settings
   * @param resource_manager The resource manager to handle resource requests
   * @param port The port to listen on (default: 8080)
   * @param max_clients Maximum number of queued client connections (default:
   * 10)
   */
  explicit TcpServer(std::shared_ptr<LocalResourceManager> resource_manager,
                     int port = 8080, int max_clients = 10)
      : resource_manager_(std::move(resource_manager)), port_(port),
        max_clients_(max_clients) {}

  ~TcpServer() = default;

  // Delete copy operations
  TcpServer(const TcpServer &) = delete;
  TcpServer &operator=(const TcpServer &) = delete;

  /**
   * @brief Main server loop accepting client connections
   *
   * Continuously accepts new client connections and spawns handler threads.
   * Each client is handled in a separate thread for concurrent downloads.
   * Loop continues until server shutdown is requested.
   */
  void run();
  void stop();
  int getServerSocket();

private:
  /**
   * @brief Initializes TCP server socket for accepting resource requests
   *
   * Creates and configures a TCP socket for accepting incoming connections.
   * Enables address reuse to handle server restarts.
   *
   * @param port Server port to listen on
   * @param max_clients Maximum number of queued client connections
   * @return Socket descriptor on success, -1 on error
   */
  int initializeSocket_(int port, int max_clients);

  /**
   * @brief Handles single client connection
   *
   * Processes resource request from connected client.
   * Sends requested resource if available.
   * Supports resuming downloads from specified offset.
   *
   * @param client_socket Socket for connected client
   */
  void handleClient_(int client_socket);

  std::shared_ptr<LocalResourceManager> resource_manager_;
  int server_socket_;
  const int port_;
  const int max_clients_;
  std::atomic<bool> should_stop_{false};
};

static void (*signal_handler(TcpServer *server))(int);
} // namespace p2p
//
