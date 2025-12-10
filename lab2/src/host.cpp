#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>

#include "common.hpp"
#include "conn.hpp"
#include "logging.hpp"
#include "wolf_game.hpp"

#ifndef TYPE_CODE
#error "TYPE_CODE must be defined"
#endif

namespace {

HostConfig parseArgs(int argc, char **argv) {
  HostConfig cfg;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--channel" && i + 1 < argc) {
      cfg.channelName = sanitizeChannelName(argv[++i]);
    } else if (arg == "--goats" && i + 1 < argc) {
      cfg.goats = std::max(1, std::atoi(argv[++i]));
    } else if (arg == "--input-timeout" && i + 1 < argc) {
      cfg.wolfInputTimeoutSec = std::max(1, std::atoi(argv[++i]));
    } else {
      std::cerr << "Неизвестный аргумент: " << arg << std::endl;
    }
  }
  return cfg;
}

} // namespace

int main(int argc, char **argv) {
  try {
    HostConfig cfg = parseArgs(argc, argv);
    std::cout << "Тип взаимодействия: " << TYPE_CODE << std::endl;
    std::cout << "Используйте канал: " << cfg.channelName << std::endl;
    std::cout << "Запустите клиента командой: client_" << TYPE_CODE << " --channel "
              << cfg.channelName << " --host-pid " << getpid() << std::endl;

    auto conn = createConnection(cfg.channelName, EndpointRole::Host);
    runHostGame(std::move(conn), cfg);
  } catch (const std::exception &ex) {
    logMessage(LogLevel::Error, std::string("Хост завершился с ошибкой: ") + ex.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}


