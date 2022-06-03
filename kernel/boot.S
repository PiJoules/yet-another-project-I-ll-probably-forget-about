// Tags are terminated by a tag of type ‘0’ and size ‘8’.
//
// If bit ‘0’ of ‘flags’ (also known as ‘optional’) is set,
// the bootloader may ignore this tag if it lacks relevant support.
// Tags are terminated by a tag of type ‘0’ and size ‘8’.
.set TAG_END, 0
.set TAG_REQUIRED, 0

.set ALIGN,    1<<0  // align loaded modules on page boundaries
.set MEMINFO,  1<<1  // provide memory map
.set GFX,      1<<2  // use gfx mode (see https://www.gnu.org/software/grub/manual/grub/html_node/gfxmode.html)

// Multiboot header constants.
.set MULTIBOOT_HEADER_MAGIC, 0x1BADB002  // 'magic number' lets bootloader find the header
.set MULTIBOOT_HEADER_FLAGS, ALIGN | MEMINFO | GFX
.set CHECKSUM, -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

// See https://www.gnu.org/software/grub/manual/multiboot/multiboot.html for
// reference of the multiboot header.
// Note this is aligned in the kernel linker script.
  .section .multiboot, "a", @progbits
  .long MULTIBOOT_HEADER_MAGIC
  .long MULTIBOOT_HEADER_FLAGS
  .long CHECKSUM

  // These aren't needed since the kernel image is in ELF format.
  .long 0x0  // header_addr
  .long 0x0  // load_addr
  .long 0x0  // load_end_addr
  .long 0x0  // bss_end_addr
  .long 0x0  // entry_addr

  // Graphics.
  .long 0x0  // 0 = linear graphics (set graphics mode)
  .long 1024  // width
  .long 768   // height
  .long 32    // depth (# of bits per pixel)

  .section .bootstrap_stack, "aw", @nobits
stack_bottom:
    .skip 16384 // 16 KiB
stack_top:

  // The linker script specifies _start as the entry point to the kernel and the
  // bootloader will jump to this position once the kernel has been loaded. It
  // doesn't make sense to return from this function as the bootloader is gone.
  .section .text
  .globl _start
_start:
  // The bootloader has loaded us into 32-bit protected mode on a x86
  // machine. Interrupts are disabled. Paging is disabled. The processor
  // state is as defined in the multiboot standard. The kernel has full
  // control of the CPU. The kernel can only make use of hardware features
  // and any code it provides as part of itself. There's no printf
  // function, unless the kernel provides its own <stdio.h> header and a
  // printf implementation. There are no security restrictions, no
  // safeguards, no debugging mechanisms, only what the kernel provides
  // itself. It has absolute and complete power over the
  // machine.

  // To set up a stack, we set the esp register to point to the top of the
  // stack (as it grows downwards on x86 systems). This is necessarily done
  // in assembly as languages such as C cannot function without a stack.
  movl $stack_top, %esp

  // Push the magic value.
  push %eax

  // Push the multiboot parameter to the kernel entry.
  push %ebx

  // Set ebp to zero so out stack tracer can stop on ebp == null.
  xor %ebp, %ebp

  // Note that interupts are still enabled when we enter here.
  call kernel_main

  // If the system has nothing more to do, put the computer into an
  // infinite loop. To do that:
  // 1) Disable interrupts with cli (clear interrupt enable in eflags).
  //    They are already disabled by the bootloader, so this is not needed.
  //    Mind that you might later enable interrupts and return from
  //    kernel_main (which is sort of nonsensical to do). Note that without this,
  //    we could hit another interrupt on the infinite loop.
  // 2) Wait for the next interrupt to arrive with hlt (halt instruction).
  //    Since they are disabled, this will lock up the computer.
  // 3) Jump to the hlt instruction if it ever wakes up due to a
  //    non-maskable interrupt occurring or due to system management mode.
  cli
hang:
  hlt
  jmp hang
