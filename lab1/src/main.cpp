#include "Daemon.hpp"
#include "Config.hpp"

#include <iostream>

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config_file>\n";
    return 1;
  }
  std::string configPath = argv[1];

  if (!Daemon::instance().init(configPath)) {
    return 1;
  }
  return Daemon::instance().run();
}


