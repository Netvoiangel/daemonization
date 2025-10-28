#include "Daemon.hpp"
#include "Config.hpp"
#include "CopyWorker.hpp"

#include <syslog.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fstream>
#include <string>

static Daemon *g_daemon_ptr = nullptr;

Daemon &Daemon::instance() { static Daemon d; return d; }

bool Daemon::init(const std::string &configPath) {
  g_daemon_ptr = this;
  openlog("daemonizer", LOG_PID | LOG_CONS, LOG_DAEMON);

  // Путь pid-файла: можно переопределить через переменную окружения DAEMONIZER_PID_FILE
  if (const char *envPid = std::getenv("DAEMONIZER_PID_FILE")) {
    pidFile = envPid;
  } else {
    pidFile = "/var/run/daemonizer.pid";
  }

  if (!ensureSingleInstance(pidFile)) {
    syslog(LOG_ERR, "Another instance is running. Exiting.");
    return false;
  }

  if (!daemonize()) {
    syslog(LOG_ERR, "Failed to daemonize");
    return false;
  }

  if (!writePidFile(pidFile)) {
    syslog(LOG_ERR, "Failed to write pid file");
    return false;
  }

  struct sigaction sa{};
  sa.sa_handler = Daemon::handleSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGHUP, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);

  if (!Config::instance().loadFromFile(configPath)) {
    syslog(LOG_ERR, "Failed to load config at startup: %s", configPath.c_str());
    return false;
  }
  syslog(LOG_INFO, "Daemon started");
  return true;
}

int Daemon::run() {
  while (!shouldTerminate) {
    if (shouldReload) {
      shouldReload = 0;
      const std::string path = Config::instance().getConfigPath();
      if (!Config::instance().loadFromFile(path)) {
        syslog(LOG_WARNING, "Reload config failed, keep previous");
      } else {
        syslog(LOG_INFO, "Config reloaded");
      }
    }

    CopyWorker::performOnce(Config::instance().getRules());

    unsigned int ms = Config::instance().getIntervalMs();
    unsigned int slept = 0;
    while (slept < ms && !shouldTerminate && !shouldReload) {
      unsigned int chunk = 200; // ms
      if (ms - slept < chunk) chunk = ms - slept;
      usleep(chunk * 1000);
      slept += chunk;
    }
  }
  syslog(LOG_INFO, "Daemon exiting");
  closelog();
  return 0;
}

void Daemon::handleSignal(int sig) {
  if (!g_daemon_ptr) return;
  if (sig == SIGHUP) {
    g_daemon_ptr->shouldReload = 1;
  } else if (sig == SIGTERM) {
    g_daemon_ptr->shouldTerminate = 1;
  }
}

bool Daemon::daemonize() {
  pid_t pid = fork();
  if (pid < 0) return false;
  if (pid > 0) _exit(0); 

  if (setsid() < 0) return false;

  pid = fork();
  if (pid < 0) return false;
  if (pid > 0) _exit(0);

  umask(0);
  chdir("/");

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  int fd = open("/dev/null", O_RDWR);
  if (fd >= 0) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) close(fd);
  }
  return true;
}

static bool pidIsRunning(pid_t pid) {
  std::string path = "/proc/" + std::to_string(pid);
  struct stat st{};
  return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool Daemon::ensureSingleInstance(const std::string &pidFilePath) {
  // Если pid-файл существует — прочитать и попытаться завершить процесс
  std::ifstream ifs(pidFilePath);
  if (ifs.is_open()) {
    pid_t oldPid = 0;
    ifs >> oldPid;
    if (oldPid > 0 && pidIsRunning(oldPid)) {
      kill(oldPid, SIGTERM);
      for (int i = 0; i < 50; ++i) {
        if (!pidIsRunning(oldPid)) break;
        usleep(100000); // 100ms
      }
      if (pidIsRunning(oldPid)) {
        syslog(LOG_ERR, "Existing process did not exit");
        return false;
      }
    }
  }
  return true;
}

bool Daemon::writePidFile(const std::string &pidFilePath) {
  std::ofstream ofs(pidFilePath, std::ios::trunc);
  if (!ofs.is_open()) return false;
  ofs << getpid();
  return ofs.good();
}


