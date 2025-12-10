#include "conn.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>
#include <string>

#include "ipc_utils.hpp"
#include "logging.hpp"

namespace {

class FifoHandles {
public:
  FifoHandles(const std::string &channel, bool owner) : owner_(owner) {
    h2cPath_ = "/tmp/" + std::string(TYPE_CODE) + "_" + channel + "_h2c.fifo";
    c2hPath_ = "/tmp/" + std::string(TYPE_CODE) + "_" + channel + "_c2h.fifo";
    if (owner_) {
      ::unlink(h2cPath_.c_str());
      ::unlink(c2hPath_.c_str());
      if (mkfifo(h2cPath_.c_str(), 0600) != 0) {
        throw std::runtime_error("mkfifo failed for " + h2cPath_);
      }
      if (mkfifo(c2hPath_.c_str(), 0600) != 0) {
        throw std::runtime_error("mkfifo failed for " + c2hPath_);
      }
    }
    h2cFd_ = ::open(h2cPath_.c_str(), O_RDWR);
    if (h2cFd_ < 0) {
      throw std::runtime_error("open failed for " + h2cPath_);
    }
    c2hFd_ = ::open(c2hPath_.c_str(), O_RDWR);
    if (c2hFd_ < 0) {
      throw std::runtime_error("open failed for " + c2hPath_);
    }
  }

  ~FifoHandles() {
    if (h2cFd_ >= 0) {
      ::close(h2cFd_);
    }
    if (c2hFd_ >= 0) {
      ::close(c2hFd_);
    }
    if (owner_) {
      ::unlink(h2cPath_.c_str());
      ::unlink(c2hPath_.c_str());
    }
  }

  FifoHandles(const FifoHandles &) = delete;
  FifoHandles &operator=(const FifoHandles &) = delete;

  FifoHandles(FifoHandles &&other) noexcept { *this = std::move(other); }
  FifoHandles &operator=(FifoHandles &&other) noexcept {
    if (this != &other) {
      this->~FifoHandles();
      h2cFd_ = other.h2cFd_;
      c2hFd_ = other.c2hFd_;
      owner_ = other.owner_;
      h2cPath_ = std::move(other.h2cPath_);
      c2hPath_ = std::move(other.c2hPath_);
      other.h2cFd_ = -1;
      other.c2hFd_ = -1;
      other.owner_ = false;
    }
    return *this;
  }

  int h2cFd() const { return h2cFd_; }
  int c2hFd() const { return c2hFd_; }

private:
  int h2cFd_{-1};
  int c2hFd_{-1};
  bool owner_{false};
  std::string h2cPath_;
  std::string c2hPath_;
};

class FifoConnection : public ConnectionEndpoint {
public:
  FifoConnection(FifoHandles handles,
                 NamedSemaphore sendSlot,
                 NamedSemaphore sendData,
                 NamedSemaphore recvSlot,
                 NamedSemaphore recvData,
                 int sendFd,
                 int recvFd)
      : handles_(std::move(handles)), sendSlot_(std::move(sendSlot)),
        sendData_(std::move(sendData)), recvSlot_(std::move(recvSlot)),
        recvData_(std::move(recvData)), sendFd_(sendFd), recvFd_(recvFd) {}

  bool send(const Message &msg) override {
    if (!sendSlot_.wait(kSemaphoreTimeoutSec)) {
      return false;
    }
    ssize_t written = ::write(sendFd_, &msg, sizeof(msg));
    if (written != static_cast<ssize_t>(sizeof(msg))) {
      logMessage(LogLevel::Error, "Не удалось записать в FIFO.");
      return false;
    }
    return sendData_.post();
  }

  bool receive(Message &msg, int timeoutSec) override {
    int effective = timeoutSec > 0 ? timeoutSec : kSemaphoreTimeoutSec;
    if (!recvData_.wait(effective)) {
      return false;
    }
    ssize_t bytes = ::read(recvFd_, &msg, sizeof(msg));
    if (bytes != static_cast<ssize_t>(sizeof(msg))) {
      logMessage(LogLevel::Error, "Не удалось прочитать из FIFO.");
      return false;
    }
    return recvSlot_.post();
  }

private:
  FifoHandles handles_;
  NamedSemaphore sendSlot_;
  NamedSemaphore sendData_;
  NamedSemaphore recvSlot_;
  NamedSemaphore recvData_;
  int sendFd_{-1};
  int recvFd_{-1};
};

NamedSemaphore makeSem(const std::string &channel,
                       const std::string &suffix,
                       bool owner,
                       unsigned int initial) {
  return NamedSemaphore(resourceName(channel, TYPE_CODE, suffix), owner, initial);
}

} // namespace

std::unique_ptr<ConnectionEndpoint>
createConnection(const std::string &channelName, EndpointRole role) {
  const std::string channel = sanitizeChannelName(channelName);
  bool owner = role == EndpointRole::Host;
  FifoHandles handles(channel, owner);

  NamedSemaphore h2cSlot = makeSem(channel, "fifo_h2c_slot", owner, 1);
  NamedSemaphore h2cData = makeSem(channel, "fifo_h2c_data", owner, 0);
  NamedSemaphore c2hSlot = makeSem(channel, "fifo_c2h_slot", owner, 1);
  NamedSemaphore c2hData = makeSem(channel, "fifo_c2h_data", owner, 0);

  if (role == EndpointRole::Host) {
    return std::make_unique<FifoConnection>(std::move(handles), std::move(h2cSlot),
                                            std::move(h2cData), std::move(c2hSlot),
                                            std::move(c2hData), handles.h2cFd(),
                                            handles.c2hFd());
  }
  return std::make_unique<FifoConnection>(std::move(handles), std::move(c2hSlot),
                                          std::move(c2hData), std::move(h2cSlot), std::move(h2cData),
                                          handles.c2hFd(), handles.h2cFd());
}


