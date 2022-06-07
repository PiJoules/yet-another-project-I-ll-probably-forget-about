#ifndef LIBC_INCLUDE_SYS_WAIT_H_
#define LIBC_INCLUDE_SYS_WAIT_H_

#include <_internals.h>
#include <bits/alltypes.h>

__BEGIN_CDECLS

pid_t wait(int *wstatus);

__END_CDECLS

#endif  // LIBC_INCLUDE_SYS_WAIT_H_
