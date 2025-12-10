#pragma once

#include "common.hpp"
#include "ipc_utils.hpp"

class MessageChannel {
public:
  MessageChannel() = default;
  MessageChannel(Message *buffer, NamedSemaphore slot, NamedSemaphore data)
      : buffer_(buffer), slot_(std::move(slot)), data_(std::move(data)) {}

  MessageChannel(const MessageChannel &) = delete;
  MessageChannel &operator=(const MessageChannel &) = delete;

  MessageChannel(MessageChannel &&) noexcept = default;
  MessageChannel &operator=(MessageChannel &&) noexcept = default;

  bool send(const Message &msg, int timeoutSec) const {
    if (!buffer_) {
      return false;
    }
    if (!slot_.wait(timeoutSec)) {
      return false;
    }
    *buffer_ = msg;
    return data_.post();
  }

  bool receive(Message &msg, int timeoutSec) const {
    if (!buffer_) {
      return false;
    }
    if (!data_.wait(timeoutSec)) {
      return false;
    }
    msg = *buffer_;
    return slot_.post();
  }

  bool valid() const { return buffer_ != nullptr; }

private:
  Message *buffer_{nullptr};
  NamedSemaphore slot_;
  NamedSemaphore data_;
};


