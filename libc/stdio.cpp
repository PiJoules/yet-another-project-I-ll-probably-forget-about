#ifndef __KERNEL__

#include <stdio.h>
#include <syscalls.h>

extern "C" int getchar() {
  char c;
  while (syscall::DebugRead(c) != K_OK) {}
  return c;
}

#endif  // __KERNEL__
