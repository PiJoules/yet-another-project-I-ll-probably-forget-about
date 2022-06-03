add_executable(test-malloc
  malloc.cpp
  ${CMAKE_SOURCE_DIR}/libc/tests/tests.cpp
  ${CMAKE_SOURCE_DIR}/libc/tests/malloc.cpp)
target_include_directories(test-malloc
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include)
target_compile_options(test-malloc
  PRIVATE ${USER_CXX_FLAGS})
target_link_libraries(test-malloc
  PRIVATE user_libc
  PRIVATE user_libcxx
)
target_link_options(test-malloc
  PRIVATE -nostdlib
)

add_to_initrd(${CMAKE_CURRENT_BINARY_DIR}/test-malloc
              "tests/test-malloc")
