enable_language(CXX ASM)
set(CMAKE_CXX_STANDARD 17)

if(NOT DEFINED KERNEL)
  set(KERNEL kernel)
endif()
set(KERNEL_DEBUG ${KERNEL}.debug)

# This is an empty source file that can be used for libraries that are linked
# together from other libs and contain no sources themselves.
file(TOUCH ${CMAKE_BINARY_DIR}/null.cpp)

# Build a libc for the kernel.
add_library(kernel_libc STATIC ${CMAKE_BINARY_DIR}/null.cpp)
target_include_directories(kernel_libc
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include/
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
  PRIVATE include/)
target_compile_options(kernel_libc PRIVATE ${KERNEL_CXX_FLAGS})
target_compile_definitions(kernel_libc PRIVATE ${KERNEL_MACRO})
target_link_libraries(kernel_libc PRIVATE common_libc_srcs)

# Build a libc++ for the kernel.
add_library(kernel_libcxx STATIC ${CMAKE_BINARY_DIR}/null.cpp)
target_include_directories(kernel_libcxx
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include/
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
  PRIVATE include/)
target_compile_options(kernel_libcxx PRIVATE ${KERNEL_CXX_FLAGS})
target_compile_definitions(kernel_libcxx PRIVATE ${KERNEL_MACRO})
target_link_libraries(kernel_libcxx PRIVATE common_libcxx_srcs)

add_library(asm_objs STATIC boot.S gdt.S isr.S switchtask.S)
target_include_directories(asm_objs PRIVATE include)

# Options for the actual kernel.
add_executable(${KERNEL_DEBUG}
  main.cpp
  serial.cpp
  multiboot.cpp
  gdt.cpp
  idt.cpp
  irq.cpp
  syscalls.cpp
  timer.cpp
  scheduler.cpp
  paging.cpp
  tests.cpp
  pmm.cpp
  kmalloc.cpp
  stacktrace.cpp
  panic.cpp
  isr.cpp
  exceptions.cpp
  channel.cpp
)

target_compile_options(${KERNEL_DEBUG} PRIVATE ${KERNEL_CXX_FLAGS})

target_include_directories(${KERNEL_DEBUG}
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include/
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
  PRIVATE include/
)
target_compile_definitions(${KERNEL_DEBUG} PRIVATE ${KERNEL_MACRO})
target_link_options(${KERNEL_DEBUG}
  PRIVATE -T ${CMAKE_CURRENT_SOURCE_DIR}/link.ld
  PRIVATE -fuse-ld=lld
  PRIVATE -nostdlib
  PRIVATE -Wl,-no-pie)
target_link_libraries(${KERNEL_DEBUG}
  PRIVATE -Wl,--start-group
  PRIVATE -Wl,--whole-archive
  PRIVATE asm_objs
  PRIVATE kernel_libc
  PRIVATE kernel_libcxx
  PRIVATE common_libc_test_srcs
  PRIVATE -Wl,--no-whole-archive
  PRIVATE -Wl,--end-group)

# Creates the stripped version without debuginfo.
add_custom_target(${KERNEL} ALL
  COMMAND ${CMAKE_OBJCOPY} --strip-all ${KERNEL_DEBUG} ${KERNEL}
  COMMAND grub-file --is-x86-multiboot ${KERNEL}

  COMMENT "Stripping ${KERNEL_DEBUG}"
  DEPENDS ${KERNEL_DEBUG}
  VERBATIM)
