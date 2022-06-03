#include <stdint.h>
#include <string.h>

extern "C" {

extern void GDTFlush(uint32_t);
extern void TSSFlush();

}  // extern "C"

namespace gdt {

namespace {

// This structure contains the value of one GDT entry.
// We use the attribute 'packed' to tell GCC not to change
// any of the alignment in the structure.
struct gdt_entry_t {
  uint16_t limit_low;   // The lower 16 bits of the limit.
  uint16_t base_low;    // The lower 16 bits of the base.
  uint8_t base_middle;  // The next 8 bits of the base.
  uint8_t access;       // Determine what ring this segment can be used in.
  uint8_t granularity;
  uint8_t base_high;  // The last 8 bits of the base.
} __attribute__((packed));
static_assert(sizeof(gdt_entry_t) == 8, "");

struct gdt_ptr_t {
  uint16_t limit;  // The upper 16 bits of all selector limits.
  uint32_t base;   // The address of the first gdt_entry_t struct.
} __attribute__((packed));

constexpr size_t kNumGDTEntries = 6;
gdt_entry_t gdt_entries[kNumGDTEntries];
gdt_ptr_t gdt_ptr;

// A struct describing a Task State Segment.
struct tss_entry_t {
  uint32_t prev_tss;  // The previous TSS - if we used hardware task switching
                      // this would form a linked list.
  uint32_t esp0;  // The stack pointer to load when we change to kernel mode.
  uint32_t ss0;   // The stack segment to load when we change to kernel mode.
  uint32_t esp1;  // Unused...
  uint32_t ss1;
  uint32_t esp2;
  uint32_t ss2;
  uint32_t cr3;
  uint32_t eip;
  uint32_t eflags;
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp;
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint32_t es;   // The value to load into ES when we change to kernel mode.
  uint32_t cs;   // The value to load into CS when we change to kernel mode.
  uint32_t ss;   // The value to load into SS when we change to kernel mode.
  uint32_t ds;   // The value to load into DS when we change to kernel mode.
  uint32_t fs;   // The value to load into FS when we change to kernel mode.
  uint32_t gs;   // The value to load into GS when we change to kernel mode.
  uint32_t ldt;  // Unused...
  uint16_t trap;
  uint16_t iomap_base;
} __attribute__((packed));
static_assert(sizeof(tss_entry_t) == 104, "");

tss_entry_t tss_entry;

// Set the value of one GDT entry.
void GDTSetGate(int32_t num, uint32_t base, uint32_t limit, uint8_t access,
                uint8_t gran) {
  gdt_entries[num].base_low = (base & 0xFFFF);
  gdt_entries[num].base_middle = (base >> 16) & 0xFF;
  gdt_entries[num].base_high = (base >> 24) & 0xFF;

  gdt_entries[num].limit_low = (limit & 0xFFFF);
  gdt_entries[num].granularity = (limit >> 16) & 0x0F;

  gdt_entries[num].granularity |= gran & 0xF0;
  gdt_entries[num].access = access;
}

void WriteTSS(int32_t num, uint16_t ss0, uint32_t esp0) {
  // Firstly, let's compute the base and limit of our entry into the GDT.
  uint32_t base = reinterpret_cast<uint32_t>(&tss_entry);
  uint32_t limit = sizeof(tss_entry);

  // Ensure the descriptor is initially zero.
  // Note that this sets the I/O privilege level in `eflags` to zero also,
  // preventing I/O instructions from being used outside of ring 0.
  memset(&tss_entry, 0, sizeof(tss_entry));

  // These values are used when an interrupt is trigerred. The CPU gets ss0 and
  // esp0 from here. ss0 is set below and used for entering ring 0 when an
  // interrupt fires. What potentially matters is esp0. When an interrupt hits,
  // the kernel switches the stack to whatever's esp0. If interrupts are enabled
  // and one triggers WHILE we are in the middle of handling an interrupt, then
  // we could clobber the old stack values that were added on from the previous
  // interrupt. This will probably only apply to syscall handling from the
  // timer and could be avoided if we just disable interrupts while in the
  // middle of an interrupt handler.
  tss_entry.ss0 = ss0;    // Set the kernel stack segment.
  tss_entry.esp0 = esp0;  // Set the kernel stack pointer.

  // Disable usage of the I/O privilege bitmap which can allow some ports to be
  // accessible outside of ring 0. We do this by setting the I/O map base to be
  // the same as the TSS limit.
  // https://cs.nyu.edu/~mwalfish/classes/15fa/ref/i386/s08_03.htm
  tss_entry.iomap_base = sizeof(tss_entry);

  // Here we set the cs, ss, ds, es, fs and gs entries in the TSS. These specify
  // what segments should be loaded when the processor switches to kernel mode.
  // Therefore they are just our normal kernel code/data segments - 0x08 and
  // 0x10 respectively, but with the last two bits set, making 0x0b and 0x13.
  // The setting of these bits sets the RPL (requested privilege level) to 3,
  // meaning that this TSS can be used to switch to kernel mode from ring 3.
  tss_entry.cs = 0x0b;
  tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs =
      0x13;

  // Now, add our TSS descriptor's address to the GDT.
  GDTSetGate(num, base, limit, 0xE9, 0x00);
}

}  // namespace

// Internal function prototypes.
void Initialize() {
  gdt_ptr.limit = (sizeof(gdt_entry_t) * kNumGDTEntries) - 1;
  gdt_ptr.base = reinterpret_cast<uint32_t>(&gdt_entries);

  GDTSetGate(0, 0, 0, 0, 0);                 // Null segment (0x00)
  GDTSetGate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);  // Code segment (0x08)
  GDTSetGate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);  // Data segment (0x10)
  GDTSetGate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);  // User mode code segment (0x18)
  GDTSetGate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);  // User mode data segment (0x20)
  WriteTSS(5, 0x10, 0);

  GDTFlush(reinterpret_cast<uint32_t>(&gdt_ptr));
  TSSFlush();
}

void SetKernelStack(uintptr_t stack) { tss_entry.esp0 = stack; }

}  // namespace gdt
