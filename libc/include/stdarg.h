#ifndef STDARG_H_
#define STDARG_H_

#include <_internals.h>

__BEGIN_CDECLS

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v) __builtin_va_end(v)
#define va_arg(v, l) __builtin_va_arg(v, l)
#define va_copy(v, l) __builtin_va_copy(v, l)

__END_CDECLS

#endif  // STDARG_H_
