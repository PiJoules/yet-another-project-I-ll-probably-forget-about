#ifndef LIBC_INCLUDE_UNISTD_H_
#define LIBC_INCLUDE_UNISTD_H_

#include <_internals.h>
#include <stdint.h>

__BEGIN_CDECLS

char *getcwd(char *buf, size_t size);
int execv(const char *path, char *const argv[]);

__END_CDECLS

#endif  // LIBC_INCLUDE_UNISTD_H_
