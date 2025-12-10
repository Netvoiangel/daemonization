#pragma once

#include <string>

enum class LogLevel {
  Info,
  Warning,
  Error,
};

void logMessage(LogLevel level, const std::string &msg);


