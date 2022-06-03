// This is the very first user program we jump to from the kernel. From here we
// can setup an environment for doing anything else in userspace. The way this
// program is loaded is that the kernel allocates a page, them memcpys this
// binary onto that page, then jumps to the start of that page. This means that
// our entry code must be at the very start of this binary. We can ensure that
// our user code is at the very start by making a flat binary. The linker script
// for userboot ensures that our binary will contain this function with this
// section at the very start of the binary.

  .global __user_program_entry
  .section .user_program_entry, "ax", @progbits
  .type __user_program_entry, @function
__user_program_entry:
  // NOTE: We do not have a stack yet when jumping here from the kernel, but
  // that should be setup already in our libc.
  jmp _start@PLT
