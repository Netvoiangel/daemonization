#pragma once

#include <semaphore.h>
#include <string>

#include "common.hpp"

void installHandshakeHandler();
bool waitForHandshake(int timeoutSec);
bool sendHandshakeSignal(pid_t pid);

class NamedSemaphore {
public:
  NamedSemaphore() = default;
  NamedSemaphore(std::string name, bool owner, unsigned int initial);
  ~NamedSemaphore();

  NamedSemaphore(const NamedSemaphore &) = delete;
  NamedSemaphore &operator=(const NamedSemaphore &) = delete;

  NamedSemaphore(NamedSemaphore &&other) noexcept;
  NamedSemaphore &operator=(NamedSemaphore &&other) noexcept;

  bool wait(int timeoutSec) const;
  bool post() const;
  bool valid() const { return handle_ != nullptr && handle_ != SEM_FAILED; }
  const std::string &name() const { return name_; }

private:
  void cleanup();

  sem_t *handle_{nullptr};
  bool owner_{false};
  std::string name_;
};


