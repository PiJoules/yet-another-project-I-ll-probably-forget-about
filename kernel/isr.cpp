#include <assert.h>
#include <kernel/idt.h>
#include <kernel/isr.h>
#include <kernel/kernel.h>
#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <kernel/scheduler.h>
#include <kernel/syscalls.h>
#include <stdio.h>
#include <stdlib.h>

namespace isr {

namespace {

constexpr uint8_t kDPLUser = 0x60;

handler_t IsrHandlers[256];

const char* exception_msgs[] = {
    "Division By Zero",
    "Debugger",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bounds",
    "Invalid Opcode",
    "Coprocessor Not Available",
    "Double fault",
    "Coprocessor Segment Overrun",
    "Invalid Task State Segment",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "Math Fault",
    "Alignement Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
};

}  // namespace

}  // namespace isr

extern "C" {
// These extern directives let us access the addresses of our ASM ISR handlers.
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

extern void isr128();

/* Calls the handler registered to a specific interrupt, if any.
 * This function is called from the real interrupt handlers set up in
 * `init_isr`, defined in `isr.S`.
 */
void isr_handler(isr::registers_t* regs) {
  assert(regs->int_no < 256);

  // printf("isr\n");
  // paging::InspectPhysicalMem(0x01400000 + 0x3000 - 8, 0x01400000 + 0x3000+8);

  // NOTE: With interrupts disabled, this means all syscalls will be blocking.
  assert(!InterruptsAreEnabled());

  if (isr::IsrHandlers[regs->int_no]) {
    isr::handler_t handler = isr::IsrHandlers[regs->int_no];
    handler(regs);
  } else {
    printf("unhandled %s %d in task %p: %s\n",
           regs->int_no < 32 ? "exception" : "interrupt", regs->int_no,
           &scheduler::GetCurrentTask(),
           regs->int_no < 32 ? isr::exception_msgs[regs->int_no] : "Unknown");

    regs->Dump();
    paging::PageDirectory4M& pd = scheduler::GetCurrentTask().getPageDir();
    pd.DumpMappedPages();
    pmm::Dump();

    // TODO: we're better than this
    abort();
  }
}

}  // extern "C"

namespace isr {

void Initialize() {
  // ISRs (raised by CPU)
  idt::IDTSetGate(0, reinterpret_cast<uint32_t>(isr0), 0x08, 0x8E);
  idt::IDTSetGate(1, reinterpret_cast<uint32_t>(isr1), 0x08, 0x8E);
  idt::IDTSetGate(2, reinterpret_cast<uint32_t>(isr2), 0x08, 0x8E);
  idt::IDTSetGate(3, reinterpret_cast<uint32_t>(isr3), 0x08, 0x8E);
  idt::IDTSetGate(4, reinterpret_cast<uint32_t>(isr4), 0x08, 0x8E);
  idt::IDTSetGate(5, reinterpret_cast<uint32_t>(isr5), 0x08, 0x8E);
  idt::IDTSetGate(6, reinterpret_cast<uint32_t>(isr6), 0x08, 0x8E);
  idt::IDTSetGate(7, reinterpret_cast<uint32_t>(isr7), 0x08, 0x8E);
  idt::IDTSetGate(8, reinterpret_cast<uint32_t>(isr8), 0x08, 0x8e);
  idt::IDTSetGate(9, reinterpret_cast<uint32_t>(isr9), 0x08, 0x8e);
  idt::IDTSetGate(10, reinterpret_cast<uint32_t>(isr10), 0x08, 0x8E);
  idt::IDTSetGate(11, reinterpret_cast<uint32_t>(isr11), 0x08, 0x8E);
  idt::IDTSetGate(12, reinterpret_cast<uint32_t>(isr12), 0x08, 0x8E);
  idt::IDTSetGate(13, reinterpret_cast<uint32_t>(isr13), 0x08, 0x8E);
  idt::IDTSetGate(14, reinterpret_cast<uint32_t>(isr14), 0x08, 0x8E);
  idt::IDTSetGate(15, reinterpret_cast<uint32_t>(isr15), 0x08, 0x8E);
  idt::IDTSetGate(16, reinterpret_cast<uint32_t>(isr16), 0x08, 0x8E);
  idt::IDTSetGate(17, reinterpret_cast<uint32_t>(isr17), 0x08, 0x8E);
  idt::IDTSetGate(18, reinterpret_cast<uint32_t>(isr18), 0x08, 0x8e);
  idt::IDTSetGate(19, reinterpret_cast<uint32_t>(isr19), 0x08, 0x8e);
  idt::IDTSetGate(20, reinterpret_cast<uint32_t>(isr20), 0x08, 0x8E);
  idt::IDTSetGate(21, reinterpret_cast<uint32_t>(isr21), 0x08, 0x8E);
  idt::IDTSetGate(22, reinterpret_cast<uint32_t>(isr22), 0x08, 0x8E);
  idt::IDTSetGate(23, reinterpret_cast<uint32_t>(isr23), 0x08, 0x8E);
  idt::IDTSetGate(24, reinterpret_cast<uint32_t>(isr24), 0x08, 0x8E);
  idt::IDTSetGate(25, reinterpret_cast<uint32_t>(isr25), 0x08, 0x8E);
  idt::IDTSetGate(26, reinterpret_cast<uint32_t>(isr26), 0x08, 0x8E);
  idt::IDTSetGate(27, reinterpret_cast<uint32_t>(isr27), 0x08, 0x8E);
  idt::IDTSetGate(28, reinterpret_cast<uint32_t>(isr28), 0x08, 0x8e);
  idt::IDTSetGate(29, reinterpret_cast<uint32_t>(isr29), 0x08, 0x8e);
  idt::IDTSetGate(39, reinterpret_cast<uint32_t>(isr30), 0x08, 0x8E);
  idt::IDTSetGate(31, reinterpret_cast<uint32_t>(isr31), 0x08, 0x8E);

  // Set the interrupt gate privilege for 0x80 to 3 so usermode can access it.
  idt::IDTSetGate(syscalls::kSyscallHandler, reinterpret_cast<uint32_t>(isr128),
                  0x08, 0x8E | kDPLUser);
}

void RegisterHandler(uint8_t num, handler_t handler) {
  assert(IsrHandlers[num] == 0 && "ISR already setup");
  IsrHandlers[num] = handler;
}

void registers_t::Dump() const {
  printf("gs:     0x%x fs:       0x%x es:     0x%x ds:  0x%x cs: 0x%x\n", gs,
         fs, es, ds, cs);
  printf("eax:    0x%x ebx:      0x%x ecx:    0x%x edx: 0x%x\n", eax, ebx, ecx,
         edx);
  printf("edi:    0x%x esi:      0x%x ebp:    0x%x esp: 0x%x\n", edi, esi, ebp,
         esp);
  printf("int_no: 0x%x err_code: 0x%x\n", int_no, err_code);
  printf("eip:    0x%x eflags:   0x%x usersp: 0x%x ss:  0x%x\n", eip, eflags,
         useresp, ss);
}

}  // namespace isr
