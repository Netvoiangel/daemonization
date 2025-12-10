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

class ShmMapping {
public:
  ShmMapping(const std::string &channel, bool owner) : owner_(owner) {
    shmName_ = resourceName(channel, TYPE_CODE, "segment");
    int flags = owner ? (O_CREAT | O_RDWR) : O_RDWR;
    fd_ = shm_open(shmName_.c_str(), flags, 0600);
    if (fd_ < 0) {
      throw std::runtime_error("shm_open failed for " + shmName_);
    }
    if (owner) {
      if (ftruncate(fd_, sizeof(SharedRegion)) != 0) {
        ::close(fd_);
        throw std::runtime_error("ftruncate failed for " + shmName_);
      }
    }
    void *addr = mmap(nullptr, sizeof(SharedRegion), PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (addr == MAP_FAILED) {
      ::close(fd_);
      throw std::runtime_error("mmap failed for " + shmName_);
    }
    region_ = static_cast<SharedRegion *>(addr);
  }

  ShmMapping() = default;
  ~ShmMapping() {
    if (region_) {
      munmap(region_, sizeof(SharedRegion));
    }
    if (fd_ >= 0) {
      ::close(fd_);
    }
    if (owner_) {
      shm_unlink(shmName_.c_str());
    }
  }

  ShmMapping(const ShmMapping &) = delete;
  ShmMapping &operator=(const ShmMapping &) = delete;

  ShmMapping(ShmMapping &&other) noexcept { *this = std::move(other); }
  ShmMapping &operator=(ShmMapping &&other) noexcept {
    if (this != &other) {
      this->~ShmMapping();
      fd_ = other.fd_;
      region_ = other.region_;
      owner_ = other.owner_;
      shmName_ = std::move(other.shmName_);
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
  std::string shmName_;
};

class SharedMemoryConnection : public ConnectionEndpoint {
public:
  SharedMemoryConnection(ShmMapping mapping, MessageChannel send, MessageChannel recv)
      : mapping_(std::move(mapping)), send_(std::move(send)), recv_(std::move(recv)) {}

  bool send(const Message &msg) override {
    return send_.send(msg, kSemaphoreTimeoutSec);
  }

  bool receive(Message &msg, int timeoutSec) override {
    int effective = timeoutSec > 0 ? timeoutSec : kSemaphoreTimeoutSec;
    return recv_.receive(msg, effective);
  }

private:
  ShmMapping mapping_;
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
  bool owner = role == EndpointRole::Host;
  ShmMapping mapping(channel, owner);
  SharedRegion *region = mapping.region();
  if (!region) {
    throw std::runtime_error("shm region is null");
  }
  if (owner) {
    std::memset(region, 0, sizeof(SharedRegion));
  }

  MessageChannel h2c = makeChannel(channel, "shm_h2c_slot", "shm_h2c_data", &region->hostToClient,
                                   owner, 1, 0);
  MessageChannel c2h = makeChannel(channel, "shm_c2h_slot", "shm_c2h_data", &region->clientToHost,
                                   owner, 1, 0);

  if (role == EndpointRole::Host) {
    return std::make_unique<SharedMemoryConnection>(std::move(mapping), std::move(h2c),
                                                    std::move(c2h));
  }
  return std::make_unique<SharedMemoryConnection>(std::move(mapping), std::move(c2h),
                                                  std::move(h2c));
}


