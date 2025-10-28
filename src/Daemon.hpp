#pragma once

#include <string>

class Daemon {
public:
  static Daemon &instance();

  // Инициализация и запуск основного цикла
  bool init(const std::string &configPath);
  int run();

  // Обработка сигналов
  static void handleSignal(int sig);

private:
  Daemon() = default;
  bool daemonize();
  bool ensureSingleInstance(const std::string &pidFilePath);
  bool writePidFile(const std::string &pidFilePath);

  volatile sig_atomic_t shouldTerminate{0};
  volatile sig_atomic_t shouldReload{0};

  std::string pidFile;
};


