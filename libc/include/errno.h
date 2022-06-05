#ifndef LIBC_INCLUDE_ERRNO_H_
#define LIBC_INCLUDE_ERRNO_H_

#include <_internals.h>
#include <bits/errno.h>

__BEGIN_CDECLS

int *__errno_location();
#define errno (*__errno_location())

__END_CDECLS

#endif  // LIBC_INCLUDE_ERRNO_H_
