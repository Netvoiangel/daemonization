#pragma once

#include <cstdint>
#include <string>

constexpr int kHandshakeCheckIntervalMs = 50;
constexpr int kHandshakeDefaultTimeoutSec = 0; // 0 == infinite
constexpr int kSemaphoreTimeoutSec = 5;

enum class MessageType : uint32_t {
  RoundStart = 1,
  GoatNumber = 2,
  Status = 3,
  Terminate = 4,
  Debug = 5,
};

enum class EndpointRole {
  Host,
  Client,
};

struct Message {
  MessageType type{MessageType::Debug};
  uint32_t round{0};
  int32_t value{0};
};

struct HostConfig {
  std::string channelName{"default"};
  int goats{1};
  int wolfInputTimeoutSec{3};
};

struct ClientConfig {
  std::string channelName{"default"};
  pid_t hostPid{-1};
};

std::string sanitizeChannelName(const std::string &raw);
std::string resourceName(const std::string &channel,
                         const std::string &typeCode,
                         const std::string &suffix);

int randomBetween(int min, int max);


