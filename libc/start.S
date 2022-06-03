#define ASM_FILE
#include <syscalls.h>
#include <status.h>

  .global _start
  .section .text
_start:
  // The first argument we receive in a new process is in EAX. Since this
  // register is needed for syscalls to setup the stack. Save it in a temporary
  // register that we can use later.
  mov %eax, %esi

  // We do not have a stack when we jump into this process for the first time.
  // First thing we'll need to do is ask for a page where we can point to one.
  mov $SYS_AllocPage, %eax
  mov $(ALLOC_ANON | ALLOC_CURRENT), %edx
  int $0x80
  // The status of this syscall is stored in EAX. If it's OK, the result is
  // in EBX.
  cmpl $K_OK, %eax
  jne 1f

  // Now get the page size, which will be stored in EAX.
  movl $SYS_PageSize, %eax
  int $0x80
  movl %ebx, %esp
  addl %eax, %esp

  pushl %esi

  call __libc_start_main@PLT

  // Pop off ESI, which is the argument passed to this process.
  addl $4, %esp

1:
  // Call the task exit syscall and wait.
  mov $SYS_ProcessKill, %eax
  int $0x80
  jmp .
