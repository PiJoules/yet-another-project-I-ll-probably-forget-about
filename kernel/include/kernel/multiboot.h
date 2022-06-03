#ifndef MULTIBOOT_H_
#define MULTIBOOT_H_

#include <assert.h>
#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

namespace multiboot {

struct multiboot_memory_map_t {
  uint32_t size;
  uint64_t addr;
  uint64_t len;
#define MULTIBOOT_MEMORY_AVAILABLE 1
#define MULTIBOOT_MEMORY_RESERVED 2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS 4
#define MULTIBOOT_MEMORY_BADRAM 5
  uint32_t type;
} __attribute__((packed));

// The symbol table for a.out.
struct multiboot_aout_symbol_table_t {
  uint32_t tabsize;
  uint32_t strsize;
  uint32_t addr;
  uint32_t reserved;
} __attribute__((packed));

/* The section header table for ELF. */
struct multiboot_elf_section_header_table_t {
  uint32_t num;
  uint32_t size;
  uint32_t addr;
  uint32_t shndx;
} __attribute__((packed));

// See https://www.gnu.org/software/grub/manual/multiboot/multiboot.html for
// information on individual fields.
struct Multiboot {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  uint32_t cmdline;
  uint32_t mods_count;
  uint32_t mods_addr;
  union {
    multiboot_aout_symbol_table_t aout_sym;
    multiboot_elf_section_header_table_t elf_sec;
  } u;
  uint32_t mmap_length;
  uint32_t mmap_addr;
  uint32_t drives_length;
  uint32_t drives_addr;
  uint32_t config_table;
  uint32_t boot_loader_name;
  uint32_t apm_table;
  uint32_t vbe_control_info;
  uint32_t vbe_mode_info;
  uint16_t vbe_mode;
  uint16_t vbe_interface_seg;
  uint16_t vbe_interface_off;
  uint16_t vbe_interface_len;

  uint64_t framebuffer_addr;
  uint32_t framebuffer_pitch;
  uint32_t framebuffer_width;
  uint32_t framebuffer_height;
  uint8_t framebuffer_bpp;
  uint8_t framebuffer_type;

  struct ModuleInfo {
    // The memory used goes from bytes ’mod_start’ to ’mod_end-1’ inclusive.
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t cmdline;  // Module command line
    uint32_t padding;  // Must be 0

    size_t getModuleSize() const { return mod_end - mod_start; }
    void *getModuleStart() const { return reinterpret_cast<void *>(mod_start); }
  };
  static_assert(sizeof(ModuleInfo) == 16, "ModuleInfo size changed!");

  bool hasModules() const { return hasFlag(3); }

  // NOTE: These getModule* functions will return jiberish if there are no
  // modules.
  ModuleInfo *getModuleBegin() const {
    return reinterpret_cast<ModuleInfo *>(mods_addr);
  }

  ModuleInfo *getModuleEnd() const { return getModuleBegin() + mods_count; }

  void Dump() const;

  bool hasFlag(uint8_t bit) const { return flags & (UINT32_C(1) << bit); }

  uint32_t getMemLower() const {
    assert(hasFlag(0));
    return mem_lower;
  }

  uint32_t getMemUpper() const {
    assert(hasFlag(0));
    return mem_upper;
  }

  bool hasMmap() const { return hasFlag(6); }

  // Copy all mmap info into a buffer provided. The number of mmaps available
  // will be recorded in `num_mmap`. Users of this function should check that
  // the number of mmaps can fit within the buffer.
  void getMmap(multiboot_memory_map_t *mmap_buffer, size_t buffer_size,
               size_t &num_mmap) const;

  multiboot_memory_map_t *getMmapAddr() const {
    return reinterpret_cast<multiboot_memory_map_t *>(mmap_addr);
  }

  using mmap_callback_t = void (*)(const multiboot_memory_map_t *mmap,
                                   void *arg);
  void IterMmap(mmap_callback_t, void *arg) const;

} __attribute__((packed));
static_assert(sizeof(Multiboot) == 110,
              "Multiboot size changed! Make sure this change was necessary "
              "then update the size checked in this assertion.");

}  // namespace multiboot

#endif
