#include "p2p-resource-sync/logger.hpp"

namespace p2p {

uint32_t Logger::node_id = 0;

void Logger::log(LogLevel level, const std::string &message) {
  std::string timestamp = getCurrentTimestamp();
  std::string output = timestamp + " [Node " + std::to_string(node_id) + "] [" +
                       levelToString(level) + "] " + message + "\n";
  fputs(output.c_str(), stderr);
  fflush(stderr);
}

const char *Logger::levelToString(LogLevel level) {
  switch (level) {
  case LogLevel::INFO:
    return "INFO";
  case LogLevel::ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

std::string Logger::getCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::ostringstream timestamp;
  timestamp << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << '.'
            << std::setfill('0') << std::setw(3) << ms.count();

  return timestamp.str();
}

} // namespace p2p
