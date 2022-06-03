#ifndef KERNEL_INCLUDE_KERNEL_TIMER_H_
#define KERNEL_INCLUDE_KERNEL_TIMER_H_

#include <kernel/isr.h>

namespace timer {

void Initialize();

// Each callback will be called on a timer click.
void RegisterTimerCallback(uint8_t num, isr::handler_t callback);
void UnregisterTimerCallback(uint8_t num);

}  // namespace timer

#endif  // KERNEL_INCLUDE_KERNEL_TIMER_H_
