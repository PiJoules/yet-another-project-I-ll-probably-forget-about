#ifndef KERNEL_INCLUDE_KERNEL_KERNEL_H_
#define KERNEL_INCLUDE_KERNEL_KERNEL_H_

#include <assert.h>
#include <stdint.h>

#include <type_traits>

// Define this in assembly because infinite loops are UB in C++.
#define LOOP_INDEFINITELY() asm volatile("cli\n 1: jmp 1b")

#define __PRINTF_LIKE__ __attribute__((format(printf, 1, 2)))

template <typename T>
constexpr inline bool IsPowerOf2(T x) {
  return x != 0 && ((x & (x - 1)) == 0);
}

template <size_t Align, typename T,
          std::enable_if_t<IsPowerOf2<T>(Align), bool> = true>
constexpr inline T RoundUp(T x) {
  return (x + (Align - 1)) & ~(Align - 1);
}

template <typename T>
constexpr inline T RoundUp(T x, size_t align) {
  assert(IsPowerOf2<T>(align));
  return (x + (static_cast<T>(align) - 1)) & ~(static_cast<T>(align) - 1);
}

inline void DisableInterrupts() { asm volatile("cli"); }
inline void EnableInterrupts() { asm volatile("sti"); }
inline bool InterruptsAreEnabled() {
  uint32_t eflags;
  asm volatile(
      "\
    pushf;\
    pop %0"
      : "=r"(eflags));
  return eflags & 0x200;
}

/**
 * RAII for disabling interrupts in a scope, then re-enabling them after exiting
 * the scope only if they were already enabled at the start.
 */
class DisableInterruptsRAII {
 public:
  DisableInterruptsRAII() : interrupts_enabled_(InterruptsAreEnabled()) {
    DisableInterrupts();
  }
  ~DisableInterruptsRAII() {
    if (interrupts_enabled_) EnableInterrupts();
  }

 private:
  bool interrupts_enabled_;
};

#endif  // KERNEL_INCLUDE_KERNEL_KERNEL_H_
