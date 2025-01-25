#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

namespace p2p {

enum class LogLevel { INFO, ERROR };

class Logger {
public:
  static uint32_t node_id;
  static void setNodeId(uint32_t id) { node_id = id; }
  static void log(LogLevel level, const std::string &message);

private:
  static const char *levelToString(LogLevel level);
  static std::string getCurrentTimestamp();
};

} // namespace p2p
