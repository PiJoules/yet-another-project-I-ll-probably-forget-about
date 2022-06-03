/*
This linker script serves two purposes:
(1) We can ensure the entry point of this program is at the very start of
   this binary. This is needed since the kernel jumps into the first user
   program at the start of the initial ramdisk.
(2) We can locate the start and end of the binary. This is needed becayse
   the way we format initrd is by appending the rest of our user filesystem
   to the end of this binary.
*/

SECTIONS {
  __USERBOOT_STAGE1_START = .;

  .text ALIGN(4):
  {
    /* Ensure the user program entry comes first. */
    *(.user_program_entry)

    /* include all other .text sections */
    *(.text)
  }

  .data ALIGN(4):
  {
    *(.data)
  }

  .rodata ALIGN(4):
  {
    *(.rodata*)
  }

  /* Read-write data (uninitialized) and stack */
  .bss : ALIGN(4K)
  {
    *(.bss)
  }

  .got.plt : {
    *(.got.plt)
  }

  /* These are here because the linker can insert these into the final
   * executable at the start, causing overlap with .text. */
  .interp : {
    *(.interp)
  }
  .note.gnu.build-id : {
    *(.note.gnu.build-id)
  }
  .note.gnu.property : {
    *(.note.gnu.property)
  }

  .last_section : {
    *(.last_section)
  }
  __USERBOOT_STAGE1_END = .;
}
