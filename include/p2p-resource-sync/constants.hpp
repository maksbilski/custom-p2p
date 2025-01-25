#pragma once

#include <chrono>
#include <cstdint>

namespace constants {

namespace announcement_broadcaster {
static constexpr std::chrono::seconds DEFAULT_BROADCAST_INTERVAL{10};

namespace socket {
static constexpr int BROADCAST_ENABLE = 1;
}
} // namespace announcement_broadcaster
namespace announcement_receiver {
static constexpr size_t MAX_DATAGRAM_SIZE = 65507;
static constexpr int DEFAULT_SOCKET_TIMEOUT_MS = 1000;
} // namespace announcement_receiver

namespace local_resource_manager {
static constexpr size_t MAX_RESOURCES = 1000;
static constexpr size_t MAX_RESOURCE_SIZE = 1024 * 1024 * 1024;
static constexpr size_t MAX_RESOURCE_NAME_LENGTH = 256;
static constexpr size_t MAX_RESOURCE_PATH_LENGTH = 4096;
} // namespace local_resource_manager

namespace resource_downloader {
static constexpr uint32_t DEFAULT_SOCKET_TIMEOUT_MS = 60000;
static constexpr int MAX_RETRIES = 5;
static constexpr size_t BUFFER_SIZE = 4096;
} // namespace resource_downloader

namespace tcp_server {
static constexpr int DEFAULT_PORT = 8080;
static constexpr int DEFAULT_MAX_CLIENTS = 10;
static constexpr size_t DEFAULT_DROP_FREQUENCY = 1000;

static constexpr size_t BUFFER_SIZE = 4096;
static constexpr std::chrono::milliseconds MAIN_LOOP_DELAY_MS =
    std::chrono::milliseconds(2000);
static constexpr int DROP_SIMULATION_INTERVAL = 5;
} // namespace tcp_server

} // namespace constants
