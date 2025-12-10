#include "ipc_utils.hpp"

#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <thread>

#include "logging.hpp"

namespace {

volatile sig_atomic_t g_handshake_received = 0;

void handshakeHandler(int) {
  g_handshake_received = 1;
}

} // namespace

void installHandshakeHandler() {
  struct sigaction sa {};
  sa.sa_handler = handshakeHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGUSR1, &sa, nullptr) != 0) {
    throw std::runtime_error("sigaction failed");
  }
}

bool waitForHandshake(int timeoutSec) {
  using namespace std::chrono;
  const auto start = steady_clock::now();
  while (!g_handshake_received) {
    std::this_thread::sleep_for(std::chrono::milliseconds(kHandshakeCheckIntervalMs));
    if (timeoutSec > 0) {
      auto elapsed = duration_cast<std::chrono::seconds>(steady_clock::now() - start).count();
      if (elapsed >= timeoutSec) {
        return false;
      }
    }
  }
  return true;
}

bool sendHandshakeSignal(pid_t pid) {
  if (pid <= 0) {
    return false;
  }
  if (kill(pid, SIGUSR1) != 0) {
    logMessage(LogLevel::Error,
               "Failed to send SIGUSR1 to host pid " + std::to_string(pid) + ": " +
                   std::strerror(errno));
    return false;
  }
  return true;
}

NamedSemaphore::NamedSemaphore(std::string name, bool owner, unsigned int initial)
    : owner_(owner), name_(std::move(name)) {
  if (owner) {
    sem_unlink(name_.c_str());
  }
  handle_ = sem_open(name_.c_str(), owner ? (O_CREAT | O_EXCL) : 0, 0600, initial);
  if (handle_ == SEM_FAILED && owner) {
    // Try to recreate without O_EXCL in case previous run left semaphore.
    handle_ = sem_open(name_.c_str(), O_CREAT, 0600, initial);
    owner_ = true;
  }
  if (handle_ == SEM_FAILED) {
    throw std::runtime_error("sem_open failed for " + name_);
  }
}

NamedSemaphore::~NamedSemaphore() {
  cleanup();
}

NamedSemaphore::NamedSemaphore(NamedSemaphore &&other) noexcept {
  *this = std::move(other);
}

NamedSemaphore &NamedSemaphore::operator=(NamedSemaphore &&other) noexcept {
  if (this != &other) {
    cleanup();
    handle_ = other.handle_;
    owner_ = other.owner_;
    name_ = std::move(other.name_);
    other.handle_ = nullptr;
    other.owner_ = false;
  }
  return *this;
}

bool NamedSemaphore::wait(int timeoutSec) const {
  if (!valid()) {
    return false;
  }
  using clock = std::chrono::steady_clock;
  const auto deadline =
      clock::now() + std::chrono::seconds(timeoutSec > 0 ? timeoutSec : kSemaphoreTimeoutSec);
  while (true) {
    if (sem_trywait(handle_) == 0) {
      return true;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno != EAGAIN) {
      return false;
    }
    if (clock::now() >= deadline) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

bool NamedSemaphore::post() const {
  if (!valid()) {
    return false;
  }
  return sem_post(handle_) == 0;
}

void NamedSemaphore::cleanup() {
  if (handle_ && handle_ != SEM_FAILED) {
    sem_close(handle_);
    if (owner_ && !name_.empty()) {
      sem_unlink(name_.c_str());
    }
  }
  handle_ = nullptr;
  owner_ = false;
}


