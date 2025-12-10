#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "common.hpp"
#include "conn.hpp"
#include "logging.hpp"
#include "wolf_game.hpp"

#ifndef TYPE_CODE
#error "TYPE_CODE must be defined"
#endif

namespace {

ClientConfig parseArgs(int argc, char **argv) {
  ClientConfig cfg;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--channel" && i + 1 < argc) {
      cfg.channelName = sanitizeChannelName(argv[++i]);
    } else if (arg == "--host-pid" && i + 1 < argc) {
      cfg.hostPid = static_cast<pid_t>(std::atoi(argv[++i]));
    } else {
      std::cerr << "Неизвестный аргумент: " << arg << std::endl;
    }
  }
  return cfg;
}

} // namespace

int main(int argc, char **argv) {
  try {
    ClientConfig cfg = parseArgs(argc, argv);
    if (cfg.hostPid <= 0) {
      throw std::runtime_error("Не указан корректный --host-pid");
    }
    std::cout << "Тип взаимодействия: " << TYPE_CODE << std::endl;
    auto conn = createConnection(cfg.channelName, EndpointRole::Client);
    runClientGame(std::move(conn), cfg);
  } catch (const std::exception &ex) {
    logMessage(LogLevel::Error, std::string("Клиент завершился с ошибкой: ") + ex.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}


