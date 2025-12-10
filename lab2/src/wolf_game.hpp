#pragma once

#include <memory>

#include "common.hpp"
#include "conn.hpp"

void runHostGame(std::unique_ptr<ConnectionEndpoint> conn, const HostConfig &config);
void runClientGame(std::unique_ptr<ConnectionEndpoint> conn, const ClientConfig &config);


