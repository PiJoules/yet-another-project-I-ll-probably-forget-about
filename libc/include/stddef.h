#ifndef LIBC_INCLUDE_STDDEF_H_
#define LIBC_INCLUDE_STDDEF_H_

#include <_internals.h>

#define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE, MEMBER)

__BEGIN_CDECLS

typedef __PTRDIFF_TYPE__ ptrdiff_t;

__END_CDECLS

#endif  // LIBC_INCLUDE_STDDEF_H_
