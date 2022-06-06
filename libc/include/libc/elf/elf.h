#ifndef LIBC_INCLUDE_LIBC_ELF_ELF_H_
#define LIBC_INCLUDE_LIBC_ELF_ELF_H_

#include <assert.h>
#include <libc/startup/globalstate.h>
#include <libc/startup/startparams.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_OK(x) DEBUG_ASSERT((x) == K_OK)

namespace libc {
namespace elf {

// Elf types and data structures are taken from
// http://www.skyfree.org/linux/references/ELF_Format.pdf.
typedef uint32_t Elf32_Addr;  // Unsigned program address
typedef uint16_t Elf32_Half;  // Unsigned medium integer
typedef uint32_t Elf32_Off;   // Unsigned file offset
typedef int32_t Elf32_Sword;  // Signed large integer
typedef int32_t Elf32_Word;   // Unsigned large integer
static_assert(sizeof(Elf32_Addr) == 4 && alignof(Elf32_Addr) == 4);
static_assert(sizeof(Elf32_Half) == 2 && alignof(Elf32_Half) == 2);
static_assert(sizeof(Elf32_Off) == 4 && alignof(Elf32_Off) == 4);
static_assert(sizeof(Elf32_Sword) == 4 && alignof(Elf32_Sword) == 4);
static_assert(sizeof(Elf32_Word) == 4 && alignof(Elf32_Word) == 4);

#define EI_NIDENT 16

// ELF header
typedef struct {
  unsigned char e_ident[16]; /* ELF identification */
  Elf32_Half e_type;         /* 2 (exec file) */
  Elf32_Half e_machine;      /* 3 (intel architecture) */
  Elf32_Word e_version;      /* 1 */
  Elf32_Addr e_entry;        /* starting point */
  Elf32_Off e_phoff;         /* program header table offset */
  Elf32_Off e_shoff;         /* section header table offset */
  Elf32_Word e_flags;        /* various flags */
  Elf32_Half e_ehsize;       /* ELF header (this) size */
  Elf32_Half e_phentsize;    /* program header table entry size */
  Elf32_Half e_phnum;        /* number of entries */
  Elf32_Half e_shentsize;    /* section header table entry size */
  Elf32_Half e_shnum;        /* number of entries */
  Elf32_Half e_shstrndx;     /* index of the section name string table */
} Elf32_Ehdr;

// Program header
typedef struct {
  Elf32_Word p_type; /* type of segment */
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
} Elf32_Phdr;

// p_type
#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_LOPROC 0x70000000
#define PT_HIPROC 0x7fffffff

#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

// Segment flags.
// https://docs.oracle.com/cd/E19683-01/816-1386/6m7qcoblk/index.html#chapter6-tbl-39
#define PF_X 1       // Executable
#define PF_W 2       // Writable
#define PF_R 4       // Readable
#define PF_MASKPROC  // Unspecified

typedef struct elf32_shdr {
  Elf32_Word sh_name;
  Elf32_Word sh_type;
  Elf32_Word sh_flags;
  Elf32_Addr sh_addr;
  Elf32_Off sh_offset;
  Elf32_Word sh_size;
  Elf32_Word sh_link;
  Elf32_Word sh_info;
  Elf32_Word sh_addralign;
  Elf32_Word sh_entsize;
} Elf32_Shdr;

typedef struct {
  Elf32_Sword d_tag;  // Dynamic array tag controls the interpretation of d_un.
  union {
    Elf32_Word d_val;  // These Elf32_Word objects represent integer values
                       // with various interpretations.

    // These Elf32_Addr objects represent program virtual addresses. As
    // mentioned previously, a file’s virtual addresses might not match the
    // memory virtual addresses during execution. When interpreting addresses
    // contained in the dynamic structure, the dynamic linker computes actual
    // addresses, based on the original file value and the memory base address.
    // For consistency, files do not contain relocation entries to ‘‘correct’’
    // addresses in the dynamic structure.
    Elf32_Addr d_ptr;
  } d_un;
} Elf32_Dyn;

typedef struct {
  // This member gives the location at which to apply the relocation action. For
  // a relocatable file, the value is the byte offset from the beginning of the
  // section to the storage unit affected by the relocation. For an executable
  // file or a shared object, the value is the virtual address of the storage
  // unit affected by the relocation.
  Elf32_Addr r_offset;

  // This member gives both the symbol table index with respect to which the
  // relocation must be made, and the type of relocation to apply. For example,
  // a call instruction’s relocation entry would hold the symbol table index of
  // the function being called. If the index is STN_UNDEF, the undefined symbol
  // index, the relocation uses 0 as the ‘‘symbol value.’’ Relocation types are
  // processor-specific. When the text refers to a relocation entry’s relocation
  // type or symbol table index, it means the result of applying ELF32_R_TYPE or
  // ELF32_R_SYM, respectively, to the entry’s r_info member.
  Elf32_Word r_info;
} Elf32_Rel;

typedef struct {
  Elf32_Addr r_offset;
  Elf32_Word r_info;

  // This member specifies a constant addend used to compute the value to be
  // stored into the relocatable field.
  Elf32_Sword r_addend;
} Elf32_Rela;

#define ELF32_R_SYM(i) ((i) >> 8)
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#define ELF32_R_INFO(s, t) (((s) << 8) + (unsigned char)(t))

typedef struct {
  // An index into the object file's symbol string table, which holds the
  // character representations of the symbol names. If the value is nonzero, it
  // represents a string table index that gives the symbol name. Otherwise, the
  // symbol table entry has no name.
  Elf32_Word st_name;
  Elf32_Addr st_value;
  Elf32_Word st_size;
  unsigned char st_info;
  unsigned char st_other;
  Elf32_Half st_shndx;
} Elf32_Sym;

// A: The addend used to compute the value of the relocatable field.
//
// B: The base address at which a shared object is loaded into memory during
// execution. Generally, a shared object file is built with a 0 base virtual
// address, but the execution address is different. See Program Header.
//
// G: The offset into the global offset table at which the address of the
// relocation entry's symbol resides during execution. See Global Offset Table
// (Processor-Specific).
//
// GOT: The address of the global offset table. See Global Offset Table
// (Processor-Specific).
//
// L: The section offset or address of the procedure linkage table entry for a
// symbol. See Procedure Linkage Table (Processor-Specific).
//
// P: The section offset or address of the storage unit being relocated,
// computed using r_offset.
//
// S: The value of the symbol whose index resides in the relocation entry.

#define R_386_RELATIVE 8  // word32; B + A

// Legal values for d_tag (dynamic entry type).
// See
// https://docs.oracle.com/cd/E23824_01/html/819-0690/chapter6-42444.html#chapter6-tbl-53
// for more flags that aren't mentioned here.
#define DT_NULL 0             /* Marks end of dynamic section */
#define DT_NEEDED 1           /* Name of needed library */
#define DT_PLTRELSZ 2         /* Size in bytes of PLT relocs */
#define DT_PLTGOT 3           /* Processor defined value */
#define DT_HASH 4             /* Address of symbol hash table */
#define DT_STRTAB 5           /* Address of string table */
#define DT_SYMTAB 6           /* Address of symbol table */
#define DT_RELA 7             /* Address of Rela relocs */
#define DT_RELASZ 8           /* Total size of Rela relocs */
#define DT_RELAENT 9          /* Size of one Rela reloc */
#define DT_STRSZ 10           /* Size of string table */
#define DT_SYMENT 11          /* Size of one symbol table entry */
#define DT_INIT 12            /* Address of init function */
#define DT_FINI 13            /* Address of termination function */
#define DT_SONAME 14          /* Name of shared object */
#define DT_RPATH 15           /* Library search path (deprecated) */
#define DT_SYMBOLIC 16        /* Start symbol search here */
#define DT_REL 17             /* Address of Rel relocs */
#define DT_RELSZ 18           /* Total size of Rel relocs */
#define DT_RELENT 19          /* Size of one Rel reloc */
#define DT_PLTREL 20          /* Type of reloc in PLT */
#define DT_DEBUG 21           /* For debugging; unspecified */
#define DT_TEXTREL 22         /* Reloc might modify .text */
#define DT_JMPREL 23          /* Address of PLT relocs */
#define DT_BIND_NOW 24        /* Process relocations of object */
#define DT_INIT_ARRAY 25      /* Array with addresses of init fct */
#define DT_FINI_ARRAY 26      /* Array with addresses of fini fct */
#define DT_INIT_ARRAYSZ 27    /* Size in bytes of DT_INIT_ARRAY */
#define DT_FINI_ARRAYSZ 28    /* Size in bytes of DT_FINI_ARRAY */
#define DT_RUNPATH 29         /* Library search path */
#define DT_FLAGS 30           /* Flags for the object being loaded */
#define DT_ENCODING 32        /* Start of encoded range */
#define DT_PREINIT_ARRAY 32   /* Array with addresses of preinit fct*/
#define DT_PREINIT_ARRAYSZ 33 /* size in bytes of DT_PREINIT_ARRAY */
#define DT_NUM 34             /* Number used */
#define DT_LOOS 0x6000000d    /* Start of OS-specific */
#define DT_HIOS 0x6ffff000    /* End of OS-specific */
#define DT_GNU_HASH \
  0x6ffffef5  // See https://flapenguin.me/elf-dt-gnu-hash (This is not formally
              // documented anywhere)
#define DT_RELCOUNT \
  0x6ffffffa  // Indicates the RELATIVE relocation count, which is produced from
              // the concatenation of all Elf32_Rel relocations.
#define DT_FLAGS_1 0x6ffffffb  // Flag values specific to this object.
#define DT_LOPROC 0x70000000   /* Start of processor-specific */
#define DT_HIPROC 0x7fffffff   /* End of processor-specific */
#define DT_PROCNUM DT_MIPS_NUM /* Most used by any processor */

// DT_FLAGS_1 values
#define DF_1_PIE 0x8000000  // Object is a position-independent executable.

class ElfModule {
 public:
  static constexpr size_t kMinAlign = alignof(Elf32_Ehdr);

  static inline bool IsValidElf(const Elf32_Ehdr *hdr) {
    return (hdr->e_ident[0] == ELFMAG0 && hdr->e_ident[1] == ELFMAG1 &&
            hdr->e_ident[2] == ELFMAG2 && hdr->e_ident[3] == ELFMAG3);
  }

  ElfModule(uintptr_t elf_data) : elf_data_(elf_data) {
    // Ensure the location of the elf file is aligned to specific data
    // structures so we can cast and dereference them easily.
    DEBUG_ASSERT(elf_data % kMinAlign == 0);

    const auto *hdr = reinterpret_cast<const Elf32_Ehdr *>(elf_data);
    DEBUG_ASSERT(IsValidElf(hdr) && "Invalid elf program");
  }

  uintptr_t getData() const { return elf_data_; }

  // Header helper functions.
  const Elf32_Ehdr *getElfHdr() const {
    return reinterpret_cast<const Elf32_Ehdr *>(elf_data_);
  }

  const Elf32_Phdr *getProgHdr() const {
    return reinterpret_cast<const Elf32_Phdr *>(elf_data_ +
                                                getElfHdr()->e_phoff);
  }

  const Elf32_Shdr *getSectionHdr() const {
    return reinterpret_cast<const Elf32_Shdr *>(elf_data_ +
                                                getElfHdr()->e_shoff);
  }

  const Elf32_Shdr *getSectionHdr(const char *section_name) const {
    const auto *shdr = getSectionHdr();
    const char *strtab = getSectionHdrStrTab();
    for (int i = 0; i < getElfHdr()->e_shnum; ++i) {
      const auto &section_hdr = shdr[i];
      const char *name = strtab + section_hdr.sh_name;
      if (strcmp(section_name, name) == 0) return &section_hdr;
    }
    return nullptr;
  };

  const Elf32_Shdr &getStrTabHdr() const {
    return getSectionHdr()[getElfHdr()->e_shstrndx];
  }

  const Elf32_Shdr *getSymTabHdr() const {
    return reinterpret_cast<const Elf32_Shdr *>(getSectionHdr(".symtab"));
  }

  // Section helper functions.
  uintptr_t getSection(const Elf32_Shdr &section_hdr) const {
    return elf_data_ + section_hdr.sh_offset;
  }

  uintptr_t getSection(const char *section_name) const {
    if (const Elf32_Shdr *section_hdr = getSectionHdr(section_name))
      return getSection(*section_hdr);
    return 0;
  }

  const char *getSectionHdrStrTab() const {
    return reinterpret_cast<const char *>(elf_data_) + getStrTabHdr().sh_offset;
  }

  const char *getStrTab() const {
    return reinterpret_cast<const char *>(getSection(".strtab"));
  }

  const Elf32_Sym *getSymTab() const {
    return reinterpret_cast<const Elf32_Sym *>(getSection(".symtab"));
  }

  const Elf32_Sym *getSymbol(const char *symbol_name) const {
    if (const auto *symtab = getSymTab()) {
      size_t symtab_size = static_cast<size_t>(getSymTabHdr()->sh_size);
      DEBUG_ASSERT(symtab_size % sizeof(Elf32_Sym) == 0 &&
                   "Symbol table size is not multiple of symbol struct.");
      size_t num_syms = symtab_size / sizeof(Elf32_Sym);
      for (int i = 0; i < num_syms; ++i) {
        const Elf32_Sym &symbol = symtab[i];
        if (strcmp(symbol_name, &getStrTab()[symbol.st_name]) == 0)
          return &symbol;
      }
    }
    return nullptr;
  }

  size_t getSymbolIndex(const Elf32_Sym &symbol) const {
    return static_cast<size_t>(reinterpret_cast<const uint8_t *>(&symbol) -
                               reinterpret_cast<const uint8_t *>(getSymTab())) /
           sizeof(Elf32_Sym);
  }

 private:
  uintptr_t elf_data_;
};

class Relocator {
 public:
  void SaveRelocs(uintptr_t rel_addr, size_t rel_size, size_t rel_ent_size) {
    DEBUG_ASSERT(!found_relocs_ && "Already found DT_REL");
    rel_addr_ = rel_addr;
    rel_size_ = rel_size;
    rel_ent_size_ = rel_ent_size;

    found_relocs_ = true;
  }

  bool FoundRelocs() const { return found_relocs_; }

  // FIXME: Would be better to move some of this into ElfModule.
  void ApplyRelocs(uintptr_t elf_data, uintptr_t this_proc_load_addr,
                   uintptr_t new_proc_load_addr) const {
    DEBUG_ASSERT(found_relocs_);
    DEBUG_ASSERT(rel_ent_size_ == sizeof(Elf32_Rel));
    DEBUG_ASSERT((elf_data + rel_addr_) % alignof(Elf32_Rel) == 0);
    DEBUG_ASSERT(rel_size_ > 0);

    auto *reloc = reinterpret_cast<Elf32_Rel *>(elf_data + rel_addr_);
    const Elf32_Rel *end =
        reinterpret_cast<Elf32_Rel *>(elf_data + rel_addr_ + rel_size_);
    do {
      // `r_offset` is the virtual address of where we need to write to.
      uintptr_t dst = this_proc_load_addr + reloc->r_offset;
      int sym = ELF32_R_SYM(reloc->r_info);
      int type = ELF32_R_TYPE(reloc->r_info);
      switch (type) {
        case R_386_RELATIVE:  // word32; B + A
          // Created by the link-editor for dynamic objects. The relocation
          // offset member gives the location within a shared object that
          // contains a value representing a relative address. The runtime
          // linker computes the corresponding virtual address by adding the
          // virtual address at which the shared object is loaded to the
          // relative address. Relocation entries for this type must specify a
          // value of zero for the symbol table index.
          DEBUG_ASSERT(sym == 0);
          *reinterpret_cast<uint32_t *>(dst) += new_proc_load_addr;
          DEBUG_PRINT("reloc R_386_RELATIVE: vaddr 0x%x => 0x%x\n", dst,
                      *reinterpret_cast<uint32_t *>(dst));
          break;
        default:
          DEBUG_PRINT("Unhandled reloc %p: sym %d, type %d\n", reloc, sym,
                      type);
          __builtin_trap();
      }
    } while (++reloc < end);
  }

 private:
  uintptr_t rel_addr_;
  size_t rel_size_, rel_ent_size_;

  bool found_relocs_ = false;
};

void LoadElfProgram(uintptr_t elf_data, const libc::startup::ArgvParam *params,
                    size_t num_params, uintptr_t vfs_data, size_t vfs_data_size,
                    const startup::Envp &envp);

}  // namespace elf
}  // namespace libc

#endif  // LIBC_INCLUDE_LIBC_ELF_ELF_H_
