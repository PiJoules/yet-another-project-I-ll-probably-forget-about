# These are source files with stuff that can be shared between userspace and
# the kernel.
add_library(common_libc_srcs INTERFACE)
target_sources(common_libc_srcs
  PUBLIC stdio.cpp
  PUBLIC string.cpp
  PUBLIC assert.cpp
  PUBLIC ctype.cpp
  PUBLIC malloc.cpp
  PUBLIC printf.cpp
  PUBLIC stdlib.cpp
)

# These are source files used for tests that can be shared between userspace
# and the kernel.
add_library(common_libc_test_srcs INTERFACE)
target_sources(common_libc_test_srcs
  PUBLIC tests/malloc.cpp
  PUBLIC tests/tests.cpp
)

# These are source files only userspace should use.
add_library(user_libc_srcs INTERFACE)
target_sources(user_libc_srcs
  PUBLIC errno.cpp
  PUBLIC getcwd.cpp
  PUBLIC globalstate.cpp
  PUBLIC startparams.cpp
  PUBLIC vfs.cpp
  PUBLIC __libc_start_main.cpp
  PUBLIC chdir.cpp
  PUBLIC elf.cpp
  PUBLIC exec.cpp
  PUBLIC getenv.cpp
  PUBLIC opendir.cpp
  PUBLIC start.S
  PUBLIC ustar.cpp
  PUBLIC wait.cpp
)
target_link_libraries(user_libc_srcs INTERFACE common_libc_srcs)
