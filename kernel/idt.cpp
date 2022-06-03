#include <stdint.h>
#include <string.h>

// extern "C" void IDTFlush(uint32_t);

namespace idt {

namespace {

// A struct describing an interrupt gate.
struct idt_entry_t {
  uint16_t base_lo;  // The lower 16 bits of the address to jump to when this
                     // interrupt fires.
  uint16_t sel;      // Kernel segment selector.
  uint8_t always0;   // This must always be zero.
  uint8_t flags;     // More flags. See documentation.
  uint16_t base_hi;  // The upper 16 bits of the address to jump to.
} __attribute__((packed));
static_assert(sizeof(idt_entry_t) == 8);

// A struct describing a pointer to an array of interrupt handlers.
// This is in a format suitable for giving to 'lidt'.
struct idt_ptr_t {
  uint16_t limit;
  uint32_t base;  // The address of the first element in our idt_entry_t array.
} __attribute__((packed));
static_assert(sizeof(idt_entry_t) == 8);

idt_entry_t idt_entries[256];
idt_ptr_t idt_ptr;

}  // namespace

void IDTSetGate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
  idt_entries[num].base_lo = base & 0xFFFF;
  idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

  idt_entries[num].sel = sel;
  idt_entries[num].always0 = 0;
  idt_entries[num].flags = flags;
}

void Initialize() {
  idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
  idt_ptr.base = reinterpret_cast<uint32_t>(&idt_entries);

  memset(&idt_entries, 0, sizeof(idt_entry_t) * 256);

  // IDTFlush(reinterpret_cast<uint32_t>(&idt_ptr));
  asm("lidt (%0)\n" ::"r"(&idt_ptr));
}

}  // namespace idt
