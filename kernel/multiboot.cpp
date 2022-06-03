#include <kernel/multiboot.h>
#include <stdio.h>
#include <string.h>

namespace multiboot {

void Multiboot::Dump() const {
  /* Print out the flags. */
  printf("flags = 0x%x\n", flags);

  /* Are mem_* valid? */
  assert(hasFlag(0) && "Need another way of getting upper and lower memory!");
  if (hasFlag(0)) {
    printf("mem_lower = %uKB, mem_upper = %uKB\n", mem_lower, mem_upper);
  }

  /* Is boot_device valid? */
  if (hasFlag(1)) printf("boot_device = 0x%x\n", boot_device);

  /* Is the command line passed? */
  if (hasFlag(2)) printf("cmdline = %s\n", (char *)cmdline);

  /* Are mods_* valid? */
  if (hasModules()) {
    ModuleInfo *mod;
    int i;

    printf("mods_count = %d, mods_addr = 0x%x\n", mods_count, mods_addr);
    for (i = 0, mod = (ModuleInfo *)mods_addr; i < mods_count; i++, mod++)
      printf(" mod_start = 0x%x, mod_end = 0x%x, cmdline = %s\n",
             (unsigned)mod->mod_start, (unsigned)mod->mod_end,
             (char *)mod->cmdline);
  }

  /* Bits 4 and 5 are mutually exclusive! */
  assert(!(hasFlag(4) && hasFlag(5)) && "Bits 4 and 5 are set.");

  /* Is the symbol table of a.out valid? */
  if (hasFlag(4)) {
    const multiboot_aout_symbol_table_t *multiboot_aout_sym = &u.aout_sym;

    printf(
        "multiboot_aout_symbol_table: tabsize = 0x%0x, "
        "strsize = 0x%x, addr = 0x%x\n",
        (unsigned)multiboot_aout_sym->tabsize,
        (unsigned)multiboot_aout_sym->strsize,
        (unsigned)multiboot_aout_sym->addr);
  }

  /* Is the section header table of ELF valid? */
  if (hasFlag(5)) {
    const multiboot_elf_section_header_table_t *multiboot_elf_sec = &u.elf_sec;

    printf(
        "multiboot_elf_sec: num = %u, size = 0x%x,"
        " addr = 0x%x, shndx = 0x%x\n",
        (unsigned)multiboot_elf_sec->num, (unsigned)multiboot_elf_sec->size,
        (unsigned)multiboot_elf_sec->addr, (unsigned)multiboot_elf_sec->shndx);
  }

  /* Are mmap_* valid? */
  if (hasMmap()) {
    printf("mmaps:\n");
    IterMmap(
        [](const multiboot_memory_map_t *mmap, void *) {
          printf(
              " size = 0x%x, base_addr = 0x%llx,"
              " length = 0x%llx, type = 0x%x\n",
              mmap->size, mmap->addr, mmap->len, mmap->type);
        },
        nullptr);
  }
}

void Multiboot::IterMmap(mmap_callback_t callback, void *arg) const {
  assert(hasMmap());
  uintptr_t addr = mmap_addr;
  uintptr_t end = mmap_addr + mmap_length;
  while (addr < end) {
    auto *mmap = reinterpret_cast<multiboot_memory_map_t *>(addr);
    callback(mmap, arg);
    addr += mmap->size + sizeof(mmap->size);
  }
}

void Multiboot::getMmap(multiboot_memory_map_t *mmap_buffer, size_t buffer_size,
                        size_t &num_mmap) const {
  assert(hasMmap());

  struct Args {
    multiboot_memory_map_t *mmap_buffer;
    const size_t buffer_size;
    size_t &num_mmap;
  };
  Args args = {
      mmap_buffer,
      buffer_size,
      num_mmap,
  };
  IterMmap(
      [](const multiboot_memory_map_t *mmap, void *arg) {
        auto *args = reinterpret_cast<Args *>(arg);

        ++(args->num_mmap);
        if (args->num_mmap * sizeof(multiboot_memory_map_t) > args->buffer_size)
          return;

        memcpy(args->mmap_buffer, mmap, sizeof(multiboot_memory_map_t));
        ++(args->mmap_buffer);
      },
      &args);
}

}  // namespace multiboot
