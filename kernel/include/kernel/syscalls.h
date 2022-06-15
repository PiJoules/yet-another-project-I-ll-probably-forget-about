#ifndef KERNEL_INCLUDE_KERNEL_SYSCALLS_H_
#define KERNEL_INCLUDE_KERNEL_SYSCALLS_H_

#include <kernel/isr.h>
#include <stdint.h>

namespace syscalls {

constexpr uint8_t kSyscallHandler = 128;

void SyscallHandler(isr::registers_t *regs);

}  // namespace syscalls

#endif  // KERNEL_INCLUDE_KERNEL_SYSCALLS_H_
