#ifndef __KERNEL__

#include <errno.h>
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

// The setenv() function shall update or add a variable in the environment of
// the calling process. The envname argument points to a string containing the
// name of an environment variable to be added or altered. The environment
// variable shall be set to the value to which envval points. The function shall
// fail if envname points to a string which contains an '=' character. If the
// environment variable named by envname already exists and the value of
// overwrite is non-zero, the function shall return success and the environment
// shall be updated.
int setenv(const char *envname, const char *envval, int overwrite) {
  // The name argument is a null pointer, points to an empty string, or points
  // to a string containing an '=' character.
  if (!envname || !*envname || strchr(envname, '=') != NULL) {
    errno = EINVAL;
    return -1;
  }

  auto *envp = libc::startup::GetEnvp();
  std::string *val = envp->getVal(envname);

  // If the environment variable named by envname already exists
  // and the value of overwrite is zero, the function shall return success and
  // the environment shall remain unchanged.
  if (val && overwrite == 0) return 0;

  // The value does not exist for this key, or it does exist but we can
  // overwrite it.
  envp->setVal(envname, envval);
  return 0;
}

#endif  // __KERNEL__
