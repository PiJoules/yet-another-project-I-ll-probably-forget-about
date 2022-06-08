# These are source files with stuff that can be shared between userspace and
# the kernel.
add_library(common_libcxx_srcs INTERFACE)
target_sources(common_libcxx_srcs
  PUBLIC new.cpp
  PUBLIC cxa_pure_virtual.cpp
)
