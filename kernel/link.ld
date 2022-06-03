/* The bootloader (grub) will look at this image and start execution at the symbol
   designated as the entry point. */
ENTRY(_start)

SECTIONS {
  /* Put sections at 4MB. */
  . = 4M;
  __KERNEL_BEGIN = .;

  /* First put the multiboot header, as it is required to be put very early
     early in the image or the bootloader won't recognize the file format. */
  .multiboot : ALIGN(4) {
    ASSERT(. == 4M, "Multiboot should be at start of kernel");
    *(.multiboot)
  }

  .text : ALIGN(4K)
  {
    *(.text)
    *(.text.*)
  }

  /* Read-only data. */
  .rodata : ALIGN(4K)
  {
      *(.rodata)
  }

  /* Read-write data (initialized) */
  .data : ALIGN(4K)
  {
      *(.data)
  }

  /* Read-write data (uninitialized) and stack */
  .bss : ALIGN(4K)
  {
      *(COMMON)
      *(.bss)
      *(.bootstrap_stack)
  }

  /* These are here because the linker can insert these into the final
   * executable at the start, causing overlap with .text. */
  .interp : {
    *(.interp)
  }
  .note.gnu.build-id : {
    *(.note.gnu.build-id)
  }

  __KERNEL_END = .;
}
