#include <kernel/kernel.h>
#include <stdio.h>

void __panic(const char *msg, const char *file, int line) {
  // We encountered a massive problem and have to stop.
  DisableInterrupts();

  printf("PANIC(%s) at %s:%d\n", msg, file, line);

  // Halt by going into an infinite loop.
  LOOP_INDEFINITELY();
  __builtin_trap();
}
