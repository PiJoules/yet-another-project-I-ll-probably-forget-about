#include <kernel/idt.h>
#include <kernel/io.h>
#include <kernel/irq.h>
#include <kernel/isr.h>

#define PIC_EOI 0x20 /* End-of-interrupt command code */

static void SendEndOfInterrupt(uint8_t irq) {
  // Send EOI to PIC2 if necessary
  if (irq >= IRQ8) io::Write8(PIC2_COMMAND, PIC_EOI);

  // Send to PIC1
  io::Write8(PIC1_COMMAND, PIC_EOI);
}

extern "C" {

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();
extern void isr_handler(isr::registers_t* regs);

/* Calls the handler associated with the calling IRQ, if any.
 */
void irq_handler(isr::registers_t* regs) {
  // uint32_t irq = regs->int_no;

  // Handle spurious interrupts
  // if (irq == IRQ7 || irq == IRQ15) {
  //    uint16_t isr = irq_get_isr();

  //    // If it's a real interrupt, the corresponding bit will be set in the
  //    ISR
  //    // Otherwise, it was probably caused by a solar flare, so do not send
  //    EOI
  //    // If faulty IRQ was sent from the slave PIC, still send EOI to the
  //    master if (!(isr & (1 << (irq - IRQ0)))) {
  //        if (irq == IRQ15) {
  //            SendEndOfInterrupt(IRQ0); // Sort of hackish
  //        }

  //        return;
  //    }
  //}

  SendEndOfInterrupt(static_cast<uint8_t>(regs->int_no));
  return isr_handler(regs);
}

}  // extern "C"

namespace irq {

namespace {

// Wait a very small amount of time (1 to 4 microseconds, generally). Useful for
// implementing a small delay for PIC remapping on old hardware or generally as
// a simple but imprecise wait. You can do an IO operation on any unused port:
// the Linux kernel by default uses port 0x80, which is often used during POST
// to log information on the motherboard's hex display but almost always unused
// after boot.
inline void Wait() { io::Write8(0x80, 0); }

/* reinitialize the PIC controllers, giving them specified vector offsets
   rather than 8h and 70h, as configured by default */

#define ICW1_ICW4 0x01      /* ICW4 (not) needed */
#define ICW1_SINGLE 0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04 /* Call address interval 4 (8) */
#define ICW1_LEVEL 0x08     /* Level triggered (edge) mode */
#define ICW1_INIT 0x10      /* Initialization - required! */

#define ICW4_8086 0x01       /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO 0x02       /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE 0x08  /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C /* Buffered mode/master */
#define ICW4_SFNM 0x10       /* Special fully nested (not) */

void RemapPic() {
  uint8_t a1 = io::Read8(PIC1_DATA);  // save masks
  uint8_t a2 = io::Read8(PIC2_DATA);

  // starts the initialization sequence (in cascade mode)
  io::Write8(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
  Wait();
  io::Write8(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
  Wait();
  io::Write8(PIC1_DATA, IRQ0);  // ICW2: Master PIC vector offset
  Wait();
  io::Write8(PIC2_DATA, IRQ8);  // ICW2: Slave PIC vector offset
  Wait();
  io::Write8(PIC1_DATA, 4);  // ICW3: tell Master PIC that there is a slave PIC
                             // at IRQ2 (0000 0100)
  Wait();

  // ICW3: tell Slave PIC its cascade identity (0000 0010)
  io::Write8(PIC2_DATA, 2);
  Wait();

  io::Write8(PIC1_DATA, ICW4_8086);
  Wait();
  io::Write8(PIC2_DATA, ICW4_8086);
  Wait();

  io::Write8(PIC1_DATA, a1);  // restore saved masks.
  io::Write8(PIC2_DATA, a2);
}

}  // namespace

void Initialize() {
  RemapPic();

  // IRQs (raised by external hardware)
  idt::IDTSetGate(32, reinterpret_cast<uint32_t>(irq0), 0x08, 0x8E);
  idt::IDTSetGate(33, reinterpret_cast<uint32_t>(irq1), 0x08, 0x8E);
  idt::IDTSetGate(34, reinterpret_cast<uint32_t>(irq2), 0x08, 0x8E);
  idt::IDTSetGate(35, reinterpret_cast<uint32_t>(irq3), 0x08, 0x8E);
  idt::IDTSetGate(36, reinterpret_cast<uint32_t>(irq4), 0x08, 0x8E);
  idt::IDTSetGate(37, reinterpret_cast<uint32_t>(irq5), 0x08, 0x8E);
  idt::IDTSetGate(38, reinterpret_cast<uint32_t>(irq6), 0x08, 0x8E);
  idt::IDTSetGate(39, reinterpret_cast<uint32_t>(irq7), 0x08, 0x8E);
  idt::IDTSetGate(40, reinterpret_cast<uint32_t>(irq8), 0x08, 0x8E);
  idt::IDTSetGate(41, reinterpret_cast<uint32_t>(irq9), 0x08, 0x8E);
  idt::IDTSetGate(42, reinterpret_cast<uint32_t>(irq10), 0x08, 0x8E);
  idt::IDTSetGate(43, reinterpret_cast<uint32_t>(irq11), 0x08, 0x8E);
  idt::IDTSetGate(44, reinterpret_cast<uint32_t>(irq12), 0x08, 0x8E);
  idt::IDTSetGate(45, reinterpret_cast<uint32_t>(irq13), 0x08, 0x8E);
  idt::IDTSetGate(46, reinterpret_cast<uint32_t>(irq14), 0x08, 0x8E);
  idt::IDTSetGate(47, reinterpret_cast<uint32_t>(irq15), 0x08, 0x8E);
}

}  // namespace irq
