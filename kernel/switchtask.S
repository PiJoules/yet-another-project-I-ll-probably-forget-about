#define ASM_FILE
#include <kernel/scheduler.h>

  // C function signature (see scheduler.cpp):
  //
  //   void switch_task(jump_args_t *args);
  //
  .global switch_task
  .section .text
switch_task:
  movl 4(%esp),%eax  // Get registers as the 1st arg (jump_args_t *).

  movw DS_OFFSET(%eax), %ds
  movw ES_OFFSET(%eax), %es
  movw FS_OFFSET(%eax), %fs
  movw GS_OFFSET(%eax), %gs

  movl EDI_OFFSET(%eax), %edi
  movl ESI_OFFSET(%eax), %esi
  movl EBP_OFFSET(%eax), %ebp
  // NOTE: We do not set esp here because we do not need to. It will be set on
  // the subsequent iret call.
  //movl ESP_OFFSET(%eax), %esp
  movl EBX_OFFSET(%eax), %ebx
  movl EDX_OFFSET(%eax), %edx
  movl ECX_OFFSET(%eax), %ecx
  //movl EAX_OFFSET(%eax), %eax  // Pop eax last so we can still access the argument.

  cmpl $0x08, CS_OFFSET(%eax)
  je 1f

  pushl $0x23   // SS - User mode data segment + 0x3 (ring 3)
  pushl USERESP_OFFSET(%eax)  // ESP; Note that ESP points to the kernel stack for exception handling
  pushl EFLAGS_OFFSET(%eax)  // EFLAGS
  pushl CS_OFFSET(%eax)   // CS - User mode code segment + 0x3 (ring 3)
  pushl EIP_OFFSET(%eax)  // EIP

  // Finally set eax.
  movl EAX_OFFSET(%eax), %eax

  iret
1:
  // If we switch to a kernel task, then that means EIP, CS, and EFLAGS were
  // pushed on the same stack used when running that task (as well as the ISR
  // number and error code). We validate this in `Schedule`. In this case, we
  // can just pop the error code and ISR number, then jump back to the previous
  // state by popping the EIP, CS, and EFLAGS which are already on the stack.
  movl ESP_OFFSET(%eax), %esp
  add $8, %esp  // Pop error code and ISR num.

  // Finally set eax.
  movl EAX_OFFSET(%eax), %eax

  // Pops EFLAGS, CS, and EIP.
  iret
