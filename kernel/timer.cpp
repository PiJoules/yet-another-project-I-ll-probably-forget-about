#include <assert.h>
#include <kernel/io.h>
#include <kernel/irq.h>
#include <kernel/isr.h>
#include <stdint.h>

#define PIT_CMD 0x43
#define PIT_SET 0x36
#define TIMER_FREQ 50  // in Hz
#define TIMER_QUOTIENT 1193180
#define PIT_0 0x40

namespace timer {

namespace {

uint32_t gTick = 0;
constexpr size_t kMaxCallbacks = 256;
isr::handler_t gCallbacks[kMaxCallbacks];

}  // namespace

void TimerCallback(isr::registers_t* regs) {
  ++gTick;

  // TODO: Maybe we could reduce this if we only have one caller?
  for (size_t i = 0; i < kMaxCallbacks; ++i) {
    if (auto callback = gCallbacks[i]) callback(regs);
  }
}

void RegisterTimerCallback(uint8_t num, isr::handler_t callback) {
  assert(!gCallbacks[num]);
  gCallbacks[num] = callback;
}

void UnregisterTimerCallback(uint8_t num) {
  assert(gCallbacks[num]);
  gCallbacks[num] = nullptr;
}

void Initialize() {
  uint32_t divisor = TIMER_QUOTIENT / TIMER_FREQ;

  io::Write8(PIT_CMD, PIT_SET);
  io::Write8(PIT_0, divisor & 0xFF);
  io::Write8(PIT_0, (divisor >> 8) & 0xFF);
}

}  // namespace timer
