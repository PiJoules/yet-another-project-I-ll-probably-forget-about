#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <new>

void *operator new(size_t size) { return malloc(size); }

void *operator new(size_t size, std::align_val_t alignment) {
  return aligned_alloc(static_cast<size_t>(alignment), size);
}

void *operator new[](size_t size) { return malloc(size); }

void *operator new[](size_t size, std::align_val_t alignment) {
  return ::operator new(size, alignment);
}

void operator delete(void *ptr) { free(ptr); }
void operator delete[](void *ptr) { free(ptr); }

void operator delete(void *ptr, [[maybe_unused]] std::align_val_t alignment) {
  free(ptr);
}
