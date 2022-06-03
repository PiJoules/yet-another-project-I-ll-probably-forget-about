#include <assert.h>
#include <ctype.h>
#include <libc/elf/elf.h>
#include <libc/startup/globalstate.h>
#include <libc/startup/startparams.h>
#include <libc/startup/vfs.h>
#include <libc/ustar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscalls.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#define DEBUG_PRINT(...) printf("DEBUG_PRINT> " __VA_ARGS__)

#ifdef NDEBUG
#define DEBUG_ASSERT(x) (void)(x)
#else
#define DEBUG_ASSERT(x) assert(x)
#endif

#define DEBUG_OK(x) DEBUG_ASSERT((x) == K_OK)

using libc::DirInfo;
using libc::FileInfo;
using libc::elf::ElfModule;
using libc::elf::LoadElfProgram;
using syscall::handle_t;

namespace {

void FindAndRunRootElfExe(uintptr_t tar_start, const char *filename,
                          const libc::startup::ArgvParam *params,
                          size_t num_params) {
  struct Args {
    FileInfo file;
    const char *filename;
  };
  Args args = {{}, filename};

  auto dircallback = [](const DirInfo &dirinfo, void *) {
    DEBUG_PRINT("dir: %s%s\n", dirinfo.prefix, dirinfo.name);
  };

  auto filecallback = [](const FileInfo &fileinfo, void *arg) {
    Args *args = reinterpret_cast<Args *>(arg);
    DEBUG_PRINT("file: %s%s\n", fileinfo.prefix, fileinfo.name);
    if (strcmp(fileinfo.name, args->filename) == 0) { args->file = fileinfo; }
  };

  libc::IterateUSTAR(tar_start, dircallback, filecallback, &args);
  if (args.file.data) {
    DEBUG_PRINT("Attempting to run %s. Allocating page for executable.\n",
                filename);

    bool aligned = true;
    uintptr_t elf_data = reinterpret_cast<uintptr_t>(args.file.data);
    if (elf_data % ElfModule::kMinAlign != 0) {
      // The raw data is not aligned to what we want. There could be a more
      // space-efficient solution, but a simple one we could do is just
      // allocate an aligned buffer and copy over the data.
      //
      // TODO: Use std::unique_ptr.
      elf_data = reinterpret_cast<uintptr_t>(
          aligned_alloc(ElfModule::kMinAlign, args.file.size));
      DEBUG_ASSERT(elf_data);
      memcpy(reinterpret_cast<void *>(elf_data), args.file.data,
             args.file.size);
      aligned = false;
    }

    LoadElfProgram(elf_data, params, num_params, /*vfs_data=*/0,
                   /*vfs_data_size=*/0);

    if (!aligned) free(reinterpret_cast<void *>(elf_data));
  } else {
    DEBUG_PRINT("Unable to locate '%s'.", filename);
  }
}

}  // namespace

#ifdef __USERBOOT_STAGE1__

extern "C" {

// NOTE: We need this here for now so the resulting binary does *not* perform
// a lookup into the GOT. A GOT is still emitted that we do a traditional call,
// pop, and add to get, but if these symbols are hidden, then the static linker
// should already know the offset between the symbol and the GOT (this results
// in a R_*_GOTOFF relocation being used rather than a R_*_GOT relocation).
//
// It's also worth noting that technically without the hidden visibility, the
// relocation used is a GOT32X reloc, which the static linker is allowed to
// optimize into a GOTOFF for a certain instruction pattern. Depending on the
// linker however, this is not always guaranteed. Just to be safe we can set
// the visibility to hidden to ensure the linker that this symbol is DSO-local.
__attribute__((visibility("hidden"))) extern uint8_t __USERBOOT_STAGE1_START,
    __USERBOOT_STAGE1_END;

}  // extern "C"

// NOTE: argc and argv are meaningless here since we jumped directly from the
// kernel, so we can ignore them here.
int main(int, char **) {
  // We're in userspace and can use *some* C++-isms. Since this is a flat
  // binary, we can't depend on ELF-isms like the GOT, .pre_init, etc. We can
  // setup an ELF loader here and load another user binary that works in a
  // normal C-like environment and ELF reader.
  //
  // The purpose of this stage userboot is to immediately jump out of it to
  // an environment that's more easily debuggable. This means, at the very
  // least, we must be able to jump into a standalone ELF program that be
  // loaded with primitive ELF-loading tools.
  printf("=== Userboot Stage 1 ===\n");
  uintptr_t userboot_start =
      reinterpret_cast<uintptr_t>(&__USERBOOT_STAGE1_START);
  uintptr_t userboot_end = reinterpret_cast<uintptr_t>(&__USERBOOT_STAGE1_END);
  size_t userboot_size = userboot_end - userboot_start;

  DEBUG_PRINT("start: %p\n", &__USERBOOT_STAGE1_START);
  DEBUG_PRINT("end: %p\n", &__USERBOOT_STAGE1_END);
  DEBUG_PRINT("size: %u bytes\n", userboot_size);
  DEBUG_PRINT("stack loc: %p\n", &userboot_start);

  // See the comment above __USERBOOT_STAGE1_START/END for this.
  DEBUG_ASSERT(
      userboot_start > 0 &&
      "Userboot starting location global does not actually point to the "
      "start of the binary");

  libc::startup::ArgvParam params[] = {
      libc::startup::ArgvParam("./test-hello"),
      libc::startup::ArgvParam("from userboot stage 1"),
  };
  FindAndRunRootElfExe(userboot_end, params[0].arg, params, 2);

  size_t tarsize = libc::GetTarsize(userboot_end);
  params[0] = libc::startup::ArgvParam("./userboot-stage2");
  params[1].arg = reinterpret_cast<char *>(userboot_end);
  params[1].size = tarsize;
  FindAndRunRootElfExe(userboot_end, params[0].arg, params, 2);
}

#else

namespace {

void Exec(const libc::startup::File &f, uintptr_t vfs_data,
          size_t vfs_data_size) {
  libc::startup::ArgvParam params[] = {
      libc::startup::ArgvParam(f.getName().c_str()),
  };
  DEBUG_PRINT("Attempting to run %s. Allocating page for executable.\n",
              f.getName().c_str());

  uintptr_t elf_data = reinterpret_cast<uintptr_t>(f.getData());
  if (elf_data % ElfModule::kMinAlign != 0) {
    std::unique_ptr<char[]> aligned_elf_data(
        new (std::align_val_t(ElfModule::kMinAlign)) char[f.getSize()]);
    DEBUG_ASSERT(aligned_elf_data);
    memcpy(aligned_elf_data.get(), f.getData(), f.getSize());
    LoadElfProgram(reinterpret_cast<uintptr_t>(aligned_elf_data.get()), params,
                   /*num_params=*/1, vfs_data, vfs_data_size);
  } else {
    LoadElfProgram(elf_data, params, /*num_params=*/1, vfs_data, vfs_data_size);
  }
}

}  // namespace

int main(int argc, char **argv) {
  // Userboot stage 2 should be nearly the exact same as userboot stage 1. The
  // main differences are:
  // (1) This is a bonafied ELF binary so it's more easily debuggable with tools
  //     like readelf and objdump.
  // (2) It *should* contain functional libc environment. It could be that we
  //     haven't fully implemented something here. *should* means that "in an
  //     ideal world, it will be supported here and not elsewhere".
  printf("=== Userboot Stage 2 ===\n");
  printf("argc: %d\n", argc);
  printf("argv: %p\n", argv);

  // NOTE: This may be unaligned.
  uintptr_t raw_vfs_data = reinterpret_cast<uintptr_t>(argv[1]);

  libc::startup::ArgvParam params[] = {
      libc::startup::ArgvParam("./test-hello"),
      libc::startup::ArgvParam("from userboot stage 2"),
  };
  FindAndRunRootElfExe(raw_vfs_data, params[0].arg, params, /*num_params=*/2);

  libc::startup::ArgvParam params2[] = {
      libc::startup::ArgvParam("./tests/test-malloc"),
  };
  FindAndRunRootElfExe(raw_vfs_data, params2[0].arg, params2, /*num_params=*/1);

  libc::startup::RootDir root;
  libc::startup::InitVFS(root, raw_vfs_data);
  root.Tree();

  // Launch a shell!
  size_t tarsize = libc::GetTarsize(raw_vfs_data);
  Exec(*root.getDir("bin")->getFile("shell"), raw_vfs_data, tarsize);
}

#endif  // __USERBOOT_STAGE1__
