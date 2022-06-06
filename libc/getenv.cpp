#ifndef __KERNEL__

#include <libc/startup/globalstate.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

char *getenv(const char *name) {
  std::string *val = libc::startup::GetEnvp()->getVal(name);
  if (!val) return NULL;
  return &(*val)[0];
}

#endif  // __KERNEL__
