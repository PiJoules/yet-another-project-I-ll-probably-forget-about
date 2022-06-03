#ifndef LIBC_INCLUDE_ASSERT_H_
#define LIBC_INCLUDE_ASSERT_H_

#include <_internals.h>

__BEGIN_CDECLS

void __assert(bool expr, const char *msg, const char *filename, int line,
              const char *pretty_func);

#ifndef STR
#define __STR(s) STR(s)
#define STR(s) #s
#endif

#ifdef NDEBUG
// Do not leave this as empty because there could be instances of an assert
// within a single line if/for/while block:
//
//   if (cond)
//     assert(x);
//   func_call();
//
#define assert(condition) ((void)0)
#else
#define assert(condition) \
  (__assert(condition, STR(condition), __FILE__, __LINE__, __PRETTY_FUNCTION__))
#endif

__END_CDECLS

#endif  // LIBC_INCLUDE_ASSERT_H_
