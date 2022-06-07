#include <assert.h>
#include <stdio.h>

void __assert(bool condition, const char *msg, const char *filename, int line,
              const char *pretty_func) {
  if (condition) return;

  printf("\n%s:%d: %s: Assertion `%s` failed.\nAborted\n", filename, line,
         pretty_func, msg);

  // NOTE: In the kernel, this will dispatch to the exception handler, wich
  // will crash and print a bunch of stuff there.
  __builtin_trap();
}
