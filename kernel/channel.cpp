#include <kernel/channel.h>
#include <kernel/scheduler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <vector>

namespace channel {
namespace {

struct Channel {
  Endpoint end1, end2;
  const scheduler::Task *owner1, *owner2;

  Channel(const scheduler::Task *owner) : owner1(owner), owner2(owner) {}
};

std::vector<Channel> *gChannels;

}  // namespace

void Endpoint::ReserveIfNeeded(size_t amt) {
  if (amt > capacity_) {
    void *newdata = malloc(amt);
    memcpy(newdata, data_, size_);
    free(data_);

    data_ = newdata;
    capacity_ = amt;
  }
}

void Endpoint::WriteSelf(const void *src, size_t size) {
  ReserveIfNeeded(size_ + size);
  memcpy(reinterpret_cast<uint8_t *>(data_) + size_, src, size);
  size_ += size;
}

bool Endpoint::ReadSelf(void *dst, size_t size, size_t *bytes_available) {
  if (size > size_) {
    if (bytes_available) *bytes_available = size_;
    return false;
  }
  memcpy(dst, data_, size);
  memmove(data_, reinterpret_cast<uint8_t *>(data_) + size, size_ - size);
  size_ -= size;
  return true;
}

void Create(Endpoint *&end1, Endpoint *&end2) {
  Channel &channel = gChannels->emplace_back(&scheduler::GetCurrentTask());
  channel.end1.Init(&channel.end2);
  channel.end2.Init(&channel.end1);
  end1 = &channel.end1;
  end2 = &channel.end2;
}

bool Endpoint::Close() {
  // This is already closed.
  if (!other_) return false;
  other_ = nullptr;

  for (auto it = gChannels->begin(); it != gChannels->end(); ++it) {
    Channel &channel = *it;
    if ((&channel.end1 == this && !channel.end2.other_) ||
        (&channel.end2 == this && !channel.end1.other_)) {
      // Both this and the other end are closed, so we can remove this channel.
      gChannels->erase(it);
      return true;
    }
  }

  return false;
}

void CloseEndpointsOwnedByTask(const scheduler::Task *owner) {
  auto it = gChannels->begin();
  auto end = gChannels->end();
  for (; it != end;) {
    Channel &channel = *it;

    bool did_remove_channel = false;
    if (channel.owner1 == owner) { did_remove_channel |= channel.end1.Close(); }
    if (channel.owner2 == owner) { did_remove_channel |= channel.end2.Close(); }

    if (did_remove_channel) {
      // It's possible that the vector now has new iterators after the erase.
      // We'll need to make new ones. This results in a costly scan.
      it = gChannels->begin();
      end = gChannels->end();
    } else {
      ++it;
    }
  }
}

void Endpoint::TransferOwner(const scheduler::Task *newowner) const {
  for (auto it = gChannels->begin(); it != gChannels->end(); ++it) {
    Channel &channel = *it;
    if (&channel.end1 == this) {
      channel.owner1 = newowner;
      return;
    } else if (&channel.end2 == this) {
      channel.owner2 = newowner;
      return;
    }
  }
}

void Initialize() { gChannels = new std::vector<Channel>; }

void Destroy() { delete gChannels; }

}  // namespace channel
