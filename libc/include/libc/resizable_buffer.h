#ifndef LIBC_INCLUDE_LIBC_RESIZABLE_BUFFER_H_
#define LIBC_INCLUDE_LIBC_RESIZABLE_BUFFER_H_

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

class ResizableBuffer {
 public:
  // TODO: This is only marked as hidden so userboot stage 1 doesn't need a
  // relocation to use this.
  __attribute__((visibility("hidden"))) static constexpr size_t kDefaultCap = 8;

  ResizableBuffer(size_t size)
      : size_(size),
        capacity_(std::max(size_, kDefaultCap)),
        data_(malloc(capacity_)) {}
  ResizableBuffer() : ResizableBuffer(0) {}
  ~ResizableBuffer() { free(data_); }

  size_t getSize() const { return size_; }
  bool empty() const { return size_ == 0; }
  size_t getCapacity() const { return capacity_; }
  void ReserveIfNeeded(size_t newsize) {
    if (newsize > capacity_) {
      void *newalloc = malloc(newsize);
      memcpy(newalloc, data_, size_);
      free(data_);
      data_ = newalloc;
      capacity_ = newsize;
    }
  }

  void Write(const void *src, size_t size) {
    ReserveIfNeeded(size + size_);
    memcpy(reinterpret_cast<uint8_t *>(data_) + size_, src, size);
    size_ += size;
  }

  template <typename T>
  void Write(const T &src) {
    Write(&src, sizeof(T));
  }

  void WriteFront(const void *src, size_t size) {
    ReserveIfNeeded(size + size_);
    memmove(reinterpret_cast<uint8_t *>(data_) + size, data_, size_);
    memcpy(data_, src, size);
    size_ += size;
  }

  void *getData() const { return data_; }
  template <typename T>
  T &get(size_t i = 0) const {
    return reinterpret_cast<T *>(data_)[i];
  }

  // Write `size` to the buffer, then `size` bytes of data.
  void WriteLenAndData(const void *src, size_t size) {
    Write(size);
    Write(src, size);
  }

  template <typename T>
  void WriteLenAndData(const T &src) {
    WriteLenAndData(&src, sizeof(T));
  }

  void Resize(size_t size) {
    if (size > size_) { ReserveIfNeeded(size); }

    // This will cut off the end if the new size is shorted.
    size_ = size;
  }

  void Read(void *dst, size_t size) {
    assert(size_ >= size);
    if (dst) { memcpy(dst, data_, size); }
    memmove(data_, reinterpret_cast<uint8_t *>(data_) + size, size_ - size);
    size_ -= size;
  }

  size_t ReadLen() {
    size_t len;
    Read(&len, sizeof(len));
    return len;
  }

  void Skip(size_t size) { Read(/*dst=*/nullptr, size); }
  void Clear() { size_ = 0; }

 private:
  size_t size_;
  size_t capacity_;
  void *data_;
};

#endif  // LIBC_INCLUDE_LIBC_RESIZABLE_BUFFER_H_
