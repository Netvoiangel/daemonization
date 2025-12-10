#include "logging.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace {

std::mutex &logMutex() {
  static std::mutex m;
  return m;
}

const char *levelToStr(LogLevel level) {
  switch (level) {
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warning:
    return "WARN";
  case LogLevel::Error:
    return "ERROR";
  }
  return "LOG";
}

std::string timestamp() {
  using clock = std::chrono::system_clock;
  auto now = clock::now();
  std::time_t tt = clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &tt);
#else
  localtime_r(&tt, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%F %T");
  return oss.str();
}

} // namespace

void logMessage(LogLevel level, const std::string &msg) {
  std::lock_guard<std::mutex> lock(logMutex());
  std::cout << "[" << timestamp() << "] "
            << "[" << levelToStr(level) << "] " << msg << std::endl;
}


