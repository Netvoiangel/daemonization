#pragma once

#include <memory>
#include <string>

#include "common.hpp"

class ConnectionEndpoint {
public:
  virtual ~ConnectionEndpoint() = default;
  virtual bool send(const Message &msg) = 0;
  virtual bool receive(Message &msg, int timeoutSec) = 0;
};

std::unique_ptr<ConnectionEndpoint>
createConnection(const std::string &channelName, EndpointRole role);


