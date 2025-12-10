#include "common.hpp"

#include <algorithm>
#include <cctype>
#include <random>

std::string sanitizeChannelName(const std::string &raw) {
  std::string sanitized;
  sanitized.reserve(raw.size());
  for (char c : raw) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
      sanitized.push_back(c);
    } else {
      sanitized.push_back('_');
    }
  }
  if (sanitized.empty()) {
    sanitized = "default";
  }
  return sanitized;
}

std::string resourceName(const std::string &channel,
                         const std::string &typeCode,
                         const std::string &suffix) {
  return "/" + typeCode + "_" + sanitizeChannelName(channel) + "_" + suffix;
}

int randomBetween(int min, int max) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> dist(min, max);
  return dist(rng);
}


