#ifndef LIBC_INCLUDE_LIBC_MALLOC_H_
#define LIBC_INCLUDE_LIBC_MALLOC_H_

#include <stdint.h>

namespace libc {
namespace malloc {

// Setup another contiguous block of memory that malloc can use. Note that
// this start block does not need to be after the previous block and may not
// be contiguous between them.
using ask_for_more_func_t = void (*)(uintptr_t &alloc, size_t &alloc_size);

// This is an incredibly simple and barebones allocator used just so I could
// get something running. It allocates everything on a contiguous block of
// memory that does not expand.
//
// Args:
//
//   alloc_start - The start of the contiguous block of memory that malloc will
//                 use.
//   alloc_size - The size of this single contiguous block of memory that
//                malloc will use.
//
void Initialize(uintptr_t alloc_start, size_t alloc_size,
                ask_for_more_func_t ask = nullptr);

void *malloc(size_t size, size_t align);
void *malloc(size_t size);
void free(void *ptr);
size_t GetAvailMemory();
void DumpAllocs();

}  // namespace malloc
}  // namespace libc

#endif  // LIBC_INCLUDE_LIBC_MALLOC_H_
