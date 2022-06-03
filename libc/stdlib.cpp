#include <_internals.h>
#include <libc/malloc.h>

#ifdef __KERNEL__
#include <kernel/kernel.h>
#include <kernel/serial.h>
#include <kernel/stacktrace.h>
#else
#include <stdio.h>
#endif

__BEGIN_CDECLS

void abort() {
#ifdef __KERNEL__
  serial::Write("KERNEL ABORT!!!\n");
  stacktrace::PrintStackTrace();
  LOOP_INDEFINITELY();
#else
  printf("ABORT!\n");
  __builtin_trap();
#endif
}

void *malloc(size_t s) { return libc::malloc::malloc(s); }
void free(void *p) { return libc::malloc::free(p); }
void *aligned_alloc(size_t align, size_t size) {
  return libc::malloc::malloc(size, align);
}

__END_CDECLS
