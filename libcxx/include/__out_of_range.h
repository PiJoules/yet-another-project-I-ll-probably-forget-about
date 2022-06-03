#ifndef LIBCXX_INCLUDE___OUT_OF_RANGE_H_
#define LIBCXX_INCLUDE___OUT_OF_RANGE_H_

#include <stdlib.h>
#include <test_macros.h>

namespace std {

inline void __throw_out_of_range() {
#ifndef _LIBCPP_NO_EXCEPTIONS
  struct out_of_range {};
  throw out_of_range();
#else
  abort();
#endif
}

}  // namespace std

#endif  // LIBCXX_INCLUDE___OUT_OF_RANGE_H_
