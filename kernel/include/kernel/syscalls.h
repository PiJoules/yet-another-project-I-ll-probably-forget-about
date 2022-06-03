#ifndef KERNEL_INCLUDE_KERNEL_SYSCALLS_H_
#define KERNEL_INCLUDE_KERNEL_SYSCALLS_H_

#include <stdint.h>

namespace syscalls {

constexpr uint8_t kSyscallHandler = 128;

void Initialize();

}  // namespace syscalls

#endif  // KERNEL_INCLUDE_KERNEL_SYSCALLS_H_
