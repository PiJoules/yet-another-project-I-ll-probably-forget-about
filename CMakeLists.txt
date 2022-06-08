cmake_minimum_required(VERSION 3.15.4)
project(discountOS)
enable_language(CXX ASM)

# Add global options.
add_compile_options(
  -Wall -Werror -Wextra -Wconversion
  -Wno-sign-compare
  -nostdinc)

# Global C++ flags.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdinc++ -std=c++17")

# Add compiler-specific flags.
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
    CMAKE_C_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-target i386-elf)
  add_link_options(-target i386-elf -fuse-ld=lld)
else()
  MESSAGE(WARNING "Using unsupported C compiler ${CMAKE_C_COMPILER_ID} or CXX compiler ${CMAKE_CXX_COMPILER_ID}")
endif()

# The kernel name.
set(KERNEL kernel)
set(IMAGE os.img)
set(KERNEL_MACRO __KERNEL__)
set(KERNEL_CXX_FLAGS
  -fno-PIC
  -ffreestanding
  -fno-exceptions
  -fno-rtti
  -ftrivial-auto-var-init=pattern

  # Disable use of simd extensions in the kernel for now.
  -mno-sse

  # This is needed for stack tracing.
  -fno-omit-frame-pointer)

set(USER_CXX_FLAGS
  -fno-exceptions
  -fno-rtti
  -fPIC

  # Disable use of simd extensions in userspace until we support it.
  -mno-sse

  # Ensure we do not emit .eh_frame.
  -fno-unwind-tables
  -fno-asynchronous-unwind-tables
)

# Expose some libc[xx] cmake variables for other subdirs to use.
add_subdirectory(libc)
add_subdirectory(libcxx)

add_subdirectory(kernel)

set(INITRD_NAME userboot.img)
add_subdirectory(userboot)

add_custom_target(${IMAGE}
  COMMAND mkdir -p isodir/boot/grub
  COMMAND cp kernel/${KERNEL} isodir/boot/${KERNEL}
  COMMAND cp ${CMAKE_SOURCE_DIR}/grub.cfg isodir/boot/grub/grub.cfg

  # Copy the ramdisk
  #COMMAND cp ${INITRD} isodir/boot/${INITRD}

  COMMAND grub-mkrescue -o ${IMAGE} isodir
  DEPENDS grub.cfg ${KERNEL} ${INITRD_NAME}
  #DEPENDS grub.cfg ${INITRD} ${KERNEL}
  COMMENT "Create cdrom image"
  VERBATIM)
