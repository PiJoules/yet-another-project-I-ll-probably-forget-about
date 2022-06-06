#ifndef LIBC_INCLUDE_UNISTD_H_
#define LIBC_INCLUDE_UNISTD_H_

#include <_internals.h>
#include <stdint.h>

__BEGIN_CDECLS

char *getcwd(char *buf, size_t size);
int execv(const char *path, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);
int chdir(const char *path);

__END_CDECLS

#endif  // LIBC_INCLUDE_UNISTD_H_
