#ifndef KERNEL_INCLUDE_KERNEL_GDT_H_
#define KERNEL_INCLUDE_KERNEL_GDT_H_

#include <stdint.h>

namespace gdt {

void Initialize();
void SetKernelStack(uintptr_t stack);

constexpr uint8_t kKernCodeSeg = 0x08;
constexpr uint8_t kKernDataSeg = 0x10;
constexpr uint8_t kUserCodeSeg = 0x18;
constexpr uint8_t kUserDataSeg = 0x20;

constexpr uint8_t kRing0 = 0;
constexpr uint8_t kRing3 = 3;

}  // namespace gdt

#endif  // KERNEL_INCLUDE_KERNEL_GDT_H_
