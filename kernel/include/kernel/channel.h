#ifndef KERNEL_INCLUDE_KERNEL_CHANNEL_H_
#define KERNEL_INCLUDE_KERNEL_CHANNEL_H_

#include <kernel/scheduler.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>

namespace channel {

class Endpoint {
 public:
  // Read off the channel `size` bytes and store them at `dst`. Return true if
  // `size` bytes were read. Otherwise, return false and set `bytes_available`
  // to the number of bytes available on the channel.
  bool Read(void *dst, size_t size, size_t *bytes_available) {
    return ReadSelf(dst, size, bytes_available);
  }

  void Write(const void *src, size_t size) {
    if (other_) other_->WriteSelf(src, size);
  }

  // Close this endpoint of the channel. Return true if the whole channel was
  // closed from this (ie. this was closed when the other end was closed).
  bool Close();

  void TransferOwner(const scheduler::Task *newowner) const;

  // FIXME: This shouldn't be default constructable.
  Endpoint() : Endpoint(0) {}
  ~Endpoint() { free(data_); }

 private:
  static constexpr size_t kDefaultCapacity = 8;
  friend void Create(Endpoint *&, Endpoint *&);

  Endpoint(size_t size)
      : size_(size),
        capacity_(std::max(size_, kDefaultCapacity)),
        data_(malloc(capacity_)) {}

  bool ReadSelf(void *dst, size_t size, size_t *bytes_available);
  void WriteSelf(const void *src, size_t size);

  void Init(Endpoint *other) { other_ = other; }

  void ReserveIfNeeded(size_t amt);
  void ShrinkCapacity(size_t amt);

  Endpoint *other_ = nullptr;
  size_t size_;
  size_t capacity_;
  void *data_;
};

void Create(Endpoint *&end1, Endpoint *&end2);
void CloseEndpointsOwnedByTask(const scheduler::Task *owner);

void Initialize();
void Destroy();

}  // namespace channel

#endif  // KERNEL_INCLUDE_KERNEL_CHANNEL_H_
