#include <assert.h>
#include <stdio.h>

#ifdef __KERNEL__
#include <kernel/kernel.h>
#include <kernel/stacktrace.h>
#endif

void __assert(bool condition, const char *msg, const char *filename, int line,
              const char *pretty_func) {
  if (condition) return;

  printf("\n%s:%d: %s: Assertion `%s` failed.\nAborted\n", filename, line,
         pretty_func, msg);

#ifdef __KERNEL__
  stacktrace::PrintStackTrace();

  // Halt by going into an infinite loop.
  LOOP_INDEFINITELY();
#else
  __builtin_trap();
#endif
}
