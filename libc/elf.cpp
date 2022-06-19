#define LOCAL_DEBUG_LVL 0

#include <assert.h>
#include <libc/elf/elf.h>
#include <libc/startup/globalstate.h>
#include <libc/startup/startparams.h>
#include <syscalls.h>

using syscall::handle_t;

namespace libc {
namespace elf {

namespace {

void HandleDynamicSection(const Elf32_Dyn *dynamic, Relocator &relocator) {
  const uintptr_t *rel_addr = nullptr;
  const int32_t *rel_size = nullptr, *rel_ent_size = nullptr;
  const int32_t *flags = nullptr, *flags1 = nullptr;
  const int32_t *rel_count;

  bool exit = false;
  while (!exit) {
    switch (dynamic->d_tag) {
      case DT_NULL:
        exit = true;
        break;  // End of .dynamic.
      case DT_STRTAB:
        // DEBUG_PRINT("String table at address 0x%x\n", dynamic->d_un.d_ptr);
        break;
      case DT_SYMTAB:
        // DEBUG_PRINT("Symbol table at address 0x%x\n", dynamic->d_un.d_ptr);
        break;
      case DT_SYMENT:
        // DEBUG_PRINT("Symbol table entry size: 0x%x\n", dynamic->d_un.d_val);
        break;
      case DT_STRSZ:
        // DEBUG_PRINT("String table size: 0x%x\n", dynamic->d_un.d_val);
        break;
      case DT_RPATH:
        DEBUG_PRINT("WARN: Unhandled RPATH offset (0x%x).\n",
                    dynamic->d_un.d_val);
        break;
      case DT_DEBUG:
        // This member is used for debugging. Its contents are not specified
        // for the ABI; programs that access this entry are not ABI-conforming.
        break;
      case DT_JMPREL:
        DEBUG_PRINT("WARN: Unhandled JMPREL (address of PLT relocs: 0x%x).\n",
                    dynamic->d_un.d_ptr);
        break;
      case DT_REL:
        // This element is similar to DT_RELA, except its table has implicit
        // addends, such as Elf32_Rel for the 32-bit file class. If this element
        // is present, the dynamic structure must also have DT_RELSZ and
        // DT_RELENT elements.
        rel_addr = &dynamic->d_un.d_ptr;
        break;
      case DT_RELSZ:
        rel_size = &dynamic->d_un.d_val;
        break;
      case DT_RELENT:
        rel_ent_size = &dynamic->d_un.d_val;
        break;
      case DT_RELCOUNT:
        rel_count = &dynamic->d_un.d_val;
        break;
      case DT_FLAGS_1:
        // https://docs.oracle.com/cd/E36784_01/html/E36857/chapter6-42444.html
        // contains info on various DT_FLAGS_1 values.
        flags1 = &dynamic->d_un.d_val;
        break;
      case DT_FLAGS:
        // https://docs.oracle.com/cd/E23824_01/html/819-0690/chapter6-42444.html#chapter7-tbl-5
        // contains info on various DT_FLAGS values.
        flags = &dynamic->d_un.d_val;
        break;
      case DT_GNU_HASH:
        // DEBUG_PRINT("GNU Hash address: 0x%x\n", dynamic->d_un.d_ptr);
        break;
      default:
        DEBUG_PRINT("Unhandled DYNAMIC tag: 0x%x\n", dynamic->d_tag);
        __builtin_trap();
    }
    ++dynamic;
  }

  if (rel_addr) {
    DEBUG_ASSERT(rel_size && rel_ent_size &&
                 "If DT_REL is present, then DT_RELSZ and DT_RELENT must also "
                 "be present.");
    DEBUG_ASSERT(*rel_size >= 0);
    DEBUG_ASSERT(*rel_ent_size >= 0);
    if (rel_count) { DEBUG_ASSERT(*rel_count * *rel_ent_size == *rel_size); }
    relocator.SaveRelocs(*rel_addr, static_cast<size_t>(*rel_size),
                         static_cast<size_t>(*rel_ent_size));
  }

  if (flags1)
    DEBUG_ASSERT((*flags1 & DF_1_PIE) &&
                 "Expected a position independent executable");

  // TODO: Might want to process these at some point.
  (void)flags;
}

}  // namespace

// NOTE: It's not guaranteed that `elf_data` will be aligned to some power of 2.
// It's either immediately concatenated after the userboot stage 1, or it's
// copied somewhere on a page.
void LoadElfProgram(uintptr_t elf_data, const libc::startup::ArgvParam *params,
                    size_t num_params, uintptr_t vfs_data, size_t vfs_data_size,
                    const startup::Envp &envp) {
  syscall::PageAlloc load_addr;

  ElfModule elf_mod(elf_data);
  DEBUG_PRINT("ELF module location: 0x%x\n", elf_data);

  // NOTE: A binary can still be marked as a shared object file (ET_DYN) yet
  // still be an executable. It depends on what the linker puts. See the
  // answers in https://stackoverflow.com/q/34519521/2775471.
  //
  // We could also probably do some inference for this. For example, if the
  // dynamic section is provided, there are some tags that are required on
  // executables or ignored on executables.
  auto *hdr = elf_mod.getElfHdr();
  DEBUG_PRINT("program type: %d\n", hdr->e_type);

  uint32_t program_entry_point = hdr->e_entry;
  DEBUG_PRINT("program entry point (offset): 0x%x\n", program_entry_point);

  // Map the loadable segments.
  const auto *phdr = elf_mod.getProgHdr();
  for (int i = 0; i < hdr->e_phnum; ++i) {
    const auto &segment = phdr[i];
    if (segment.p_type != PT_LOAD) continue;
    DEBUG_PRINT(
        "LOAD segment Offset: %x, VirtAddr: %p, filesz: 0x%x, memsz: 0x%x\n",
        segment.p_offset, (void *)segment.p_vaddr, segment.p_filesz,
        segment.p_memsz);

    // NOTE: This assumes the binary is PIC. This also doesn't take into account
    // segment flags.
    // If the segment’s memory size (p_memsz) is larger than the file size
    // (p_filesz), the extra bytes are defined to hold the value 0 and to
    // follow the segment’s initialized area.
    uint8_t *dst =
        reinterpret_cast<uint8_t *>(load_addr.getAddr()) + segment.p_vaddr;
    uint8_t *src = reinterpret_cast<uint8_t *>(elf_data) + segment.p_offset;
    if (segment.p_memsz > segment.p_filesz) {
      size_t cpy_size = static_cast<size_t>(segment.p_filesz);
      memcpy(dst, src, cpy_size);
      memset(dst + cpy_size, 0,
             static_cast<size_t>(segment.p_memsz) - cpy_size);
    } else {
      memcpy(dst, src, static_cast<size_t>(segment.p_memsz));
    }
  }

  // Handle the DYNAMIC segment.
  // See http://www.skyfree.org/linux/references/ELF_Format.pdf on how to handle
  // each tag.
  // https://docs.oracle.com/cd/E19957-01/806-0641/chapter6-42444/index.html
  // contains others.
  Relocator relocator;
  for (int i = 0; i < hdr->e_phnum; ++i) {
    if (phdr[i].p_type == PT_DYNAMIC) {
      // TODO: Could probably do some error checking here by finding the
      // .dynamic section and the _DYNAMIC symbol to ensure this points to the
      // right location.
      HandleDynamicSection(
          reinterpret_cast<const Elf32_Dyn *>(elf_data + phdr[i].p_offset),
          relocator);
      break;
    }
  }

  // Create a new process.
  handle_t proc_handle;
  DEBUG_OK(syscall::ProcessCreate(proc_handle));
  DEBUG_PRINT("New process handle: 0x%x\n", proc_handle);

  // Map a the page we allocated into the new process's address space.
  // Here it'll just be the same virtual address. The SWAP_OWNER flag means
  // the new process will own the physical page backing this virtual page.
  uintptr_t new_load_addr;
  DEBUG_OK(load_addr.MapAnonAndSwap(proc_handle, new_load_addr));
  DEBUG_PRINT(
      "New process load address = 0x%x, mapped from this process at 0x%x\n",
      new_load_addr, load_addr.getAddr());

  // Apply relocations using the new process' load address.
  if (relocator.FoundRelocs()) {
    relocator.ApplyRelocs(elf_data, load_addr.getAddr(), new_load_addr);
  }

  handle_t end1, end2;
  syscall::ChannelCreate(end1, end2);
  syscall::TransferHandle(proc_handle, end2);

  syscall::ChannelWrite(end1, &vfs_data_size, sizeof(vfs_data_size));
  syscall::ChannelWrite(end1, reinterpret_cast<void *>(vfs_data),
                        vfs_data_size);

  // First write the size of the buffer so the receiving end knows how much to
  // expect.
  ResizableBuffer buffer;
  envp.Pack(buffer);
  size_t envpsize = buffer.getSize();
  syscall::ChannelWrite(end1, &envpsize, sizeof(envpsize));

  // Then write the rest of the data.
  syscall::ChannelWrite(end1, buffer.getData(), envpsize);

  // Write the argv data.
  buffer.Clear();
  buffer.Resize(libc::startup::PackSize(params, num_params));
  libc::startup::PackParams(reinterpret_cast<uintptr_t>(buffer.getData()),
                            params, num_params);
  size_t argvsize = buffer.getSize();
  syscall::ChannelWrite(end1, &argvsize, sizeof(argvsize));
  syscall::ChannelWrite(end1, buffer.getData(), argvsize);

  // Start the process.
  syscall::ProcessStart(proc_handle, new_load_addr + program_entry_point, end2);

  syscall::HandleClose(end1);
}

}  // namespace elf
}  // namespace libc
