#ifndef KERNEL_INCLUDE_KERNEL_KMALLOC_H_
#define KERNEL_INCLUDE_KERNEL_KMALLOC_H_

#include <libc/malloc.h>
#include <stdint.h>

namespace kmalloc {

void Initialize();

inline void *kmalloc(size_t size, size_t align) {
  return libc::malloc::malloc(size, align);
}

inline void *kmalloc(size_t size) { return libc::malloc::malloc(size); }

inline void kfree(void *ptr) { return libc::malloc::free(ptr); }

inline size_t GetAvailMemory() { return libc::malloc::GetAvailMemory(); }

inline void DumpAllocs() { return libc::malloc::DumpAllocs(); }

}  // namespace kmalloc

#endif  // KERNEL_INCLUDE_KERNEL_KMALLOC_H_
