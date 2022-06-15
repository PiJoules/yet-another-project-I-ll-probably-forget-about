#ifndef KERNEL_INCLUDE_KERNEL_ISR_H_
#define KERNEL_INCLUDE_KERNEL_ISR_H_

#include <stdint.h>

namespace isr {

enum Exception {
  kDivideByZero = 0,
  kDebug = 1,
  kNonMaskableInterrupt = 2,
  kBreakpoint = 3,
  kOverflow = 4,
  kBoundRangeExceeded = 5,
  kInvalidOpcode = 6,
  kDeviceNotAvailable = 7,
  kDoubleFault = 8,
  kCoprocessorSegmentOverrun = 9,
  kInvalidTSS = 10,
  kSegmentNotPresent = 11,
  kStackSegmentFault = 12,
  kGeneralProtectionFault = 13,
  kPageFault = 14,

  // 15 is reserved.

  kx87FloatingPointException = 16,
  kAlignmentCheck = 17,
  kMachineCheck = 18,
  kSIMDFloatingPointException = 19,
  kVirtualizationException = 20,
  kControlProtectionException = 21,

  // 22-27 are reserved.

  kHypervisorInjectionException = 28,
  kVMMCommunicationException = 29,
  kSecurityException = 30,

  // 31 is reserved.
};

struct registers_t {
  uint16_t gs, fs, es, ds;
  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
  uint32_t int_no, err_code;

  // Pushed by the processor automatically.
  // If the iret is inter-privilege, then iret only pops EIP, CS, and EFLAGS
  // from the stack. Otherwise for intra-privilege, iret additionally pops the
  // stack pointer and SS.
  uint32_t eip, cs, eflags, useresp, ss;

  void Dump() const;
};

using handler_t = void (*)(registers_t *);

void RegisterIsrHandler(uint8_t num, handler_t handler);
void Initialize();
const char *ExceptionStr(uint32_t int_no);

constexpr size_t kNumIsrHandlers = 256;

}  // namespace isr

#endif  // KERNEL_INCLUDE_KERNEL_ISR_H_
