#ifndef LIBC_INCLUDE_CTYPE_H_
#define LIBC_INCLUDE_CTYPE_H_

#include <_internals.h>

__BEGIN_CDECLS

bool isspace(char c);
bool isprint(int ch);

__END_CDECLS

#endif  // LIBC_INCLUDE_CTYPE_H_
