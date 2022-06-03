#ifndef KERNEL_INCLUDE_KERNEL_IDT_H_
#define KERNEL_INCLUDE_KERNEL_IDT_H_

#include <stdint.h>

namespace idt {

void Initialize();
void IDTSetGate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

}  // namespace idt

#endif  // KERNEL_INCLUDE_KERNEL_IDT_H_
