#include "conn.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>
#include <string>

#include "channel.hpp"
#include "logging.hpp"

namespace {

struct SharedRegion {
  Message hostToClient;
  Message clientToHost;
};

class MemoryMapping {
public:
  MemoryMapping(const std::string &path, bool owner) : owner_(owner), path_(path) {
    fd_ = ::open(path.c_str(), O_RDWR | O_CREAT, 0600);
    if (fd_ < 0) {
      throw std::runtime_error("Не удалось открыть файл " + path);
    }
    if (ftruncate(fd_, sizeof(SharedRegion)) != 0) {
      ::close(fd_);
      throw std::runtime_error("Не удалось изменить размер файла " + path);
    }
    void *addr = mmap(nullptr, sizeof(SharedRegion), PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (addr == MAP_FAILED) {
      ::close(fd_);
      throw std::runtime_error("Не удалось создать отображение файла " + path);
    }
    region_ = static_cast<SharedRegion *>(addr);
  }

  MemoryMapping() = default;
  ~MemoryMapping() {
    if (region_) {
      munmap(region_, sizeof(SharedRegion));
    }
    if (fd_ >= 0) {
      ::close(fd_);
    }
    if (owner_ && !path_.empty()) {
      ::unlink(path_.c_str());
    }
  }

  MemoryMapping(const MemoryMapping &) = delete;
  MemoryMapping &operator=(const MemoryMapping &) = delete;

  MemoryMapping(MemoryMapping &&other) noexcept { *this = std::move(other); }
  MemoryMapping &operator=(MemoryMapping &&other) noexcept {
    if (this != &other) {
      this->~MemoryMapping();
      fd_ = other.fd_;
      region_ = other.region_;
      owner_ = other.owner_;
      path_ = std::move(other.path_);
      other.fd_ = -1;
      other.region_ = nullptr;
      other.owner_ = false;
    }
    return *this;
  }

  SharedRegion *region() const { return region_; }

private:
  int fd_{-1};
  SharedRegion *region_{nullptr};
  bool owner_{false};
  std::string path_;
};

class SharedMemoryConnection : public ConnectionEndpoint {
public:
  SharedMemoryConnection(MemoryMapping mapping, MessageChannel send, MessageChannel recv)
      : mapping_(std::move(mapping)), send_(std::move(send)), recv_(std::move(recv)) {}

  bool send(const Message &msg) override {
    return send_.send(msg, kSemaphoreTimeoutSec);
  }

  bool receive(Message &msg, int timeoutSec) override {
    int effective = timeoutSec > 0 ? timeoutSec : kSemaphoreTimeoutSec;
    return recv_.receive(msg, effective);
  }

private:
  MemoryMapping mapping_;
  MessageChannel send_;
  MessageChannel recv_;
};

MessageChannel makeChannel(const std::string &channel,
                           const std::string &suffixSlot,
                           const std::string &suffixData,
                           Message *buffer,
                           bool owner,
                           unsigned int slotInit,
                           unsigned int dataInit) {
  NamedSemaphore slot(resourceName(channel, TYPE_CODE, suffixSlot), owner, slotInit);
  NamedSemaphore data(resourceName(channel, TYPE_CODE, suffixData), owner, dataInit);
  return MessageChannel(buffer, std::move(slot), std::move(data));
}

} // namespace

std::unique_ptr<ConnectionEndpoint>
createConnection(const std::string &channelName, EndpointRole role) {
  const std::string channel = sanitizeChannelName(channelName);
  const std::string filePath = "/tmp/" + std::string(TYPE_CODE) + "_" + channel + ".bin";
  bool owner = role == EndpointRole::Host;
  MemoryMapping mapping(filePath, owner);
  SharedRegion *region = mapping.region();
  if (!region) {
    throw std::runtime_error("Не удалось получить разделяемую область");
  }

  if (owner) {
    std::memset(region, 0, sizeof(SharedRegion));
  }

  MessageChannel h2c = makeChannel(channel, "h2c_slot", "h2c_data", &region->hostToClient, owner, 1, 0);
  MessageChannel c2h =
      makeChannel(channel, "c2h_slot", "c2h_data", &region->clientToHost, owner, 1, 0);

  if (role == EndpointRole::Host) {
    return std::make_unique<SharedMemoryConnection>(std::move(mapping), std::move(h2c),
                                                    std::move(c2h));
  }
  return std::make_unique<SharedMemoryConnection>(std::move(mapping), std::move(c2h),
                                                  std::move(h2c));
}


