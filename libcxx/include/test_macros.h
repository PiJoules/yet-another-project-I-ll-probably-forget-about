#ifndef LIBCXX_INCLUDE_TEST_MACROS_H_
#define LIBCXX_INCLUDE_TEST_MACROS_H_

#ifndef __has_feature
#define __has_feature(x) 0
#warning "__has_feature not implemented with this compiler"
#endif

#if !__has_feature(cxx_exceptions)
#define _LIBCPP_NO_EXCEPTIONS
#endif

#endif  // LIBCXX_INCLUDE_TEST_MACROS_H_
