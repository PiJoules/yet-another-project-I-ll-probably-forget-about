#define LOCAL_DEBUG_LVL 0

#include <assert.h>
#include <ctype.h>
#include <libc/elf/elf.h>
#include <libc/startup/startparams.h>
#include <libc/ustar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>

#define DEBUG_OK(x) DEBUG_ASSERT((x) == K_OK)

#ifdef __USERBOOT_STAGE1__

using libc::DirInfo;
using libc::FileInfo;
using libc::elf::ElfModule;
using libc::elf::LoadElfProgram;

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

  size_t tarsize =
      libc::IterateUSTAR(tar_start, dircallback, filecallback, &args);

  // NOTE: This means userboot stage 2 starts with no env variables.
  libc::startup::Envp envp;

  if (args.file.data) {
    DEBUG_PRINT("Attempting to run %s. Allocating page for executable.\n",
                filename);

    uintptr_t elf_data = reinterpret_cast<uintptr_t>(args.file.data);
    if (elf_data % ElfModule::kMinAlign != 0) {
      // The raw data is not aligned to what we want. There could be a more
      // space-efficient solution, but a simple one we could do is just
      // allocate an aligned buffer and copy over the data.
      std::unique_ptr<char[]> aligned_elf_data(
          new (std::align_val_t(ElfModule::kMinAlign)) char[args.file.size]);
      DEBUG_ASSERT(elf_data);
      memcpy(aligned_elf_data.get(), args.file.data, args.file.size);
      LoadElfProgram(reinterpret_cast<uintptr_t>(aligned_elf_data.get()),
                     params, num_params, tar_start, tarsize, envp);
    } else {
      LoadElfProgram(elf_data, params, num_params, tar_start, tarsize, envp);
    }
  } else {
    DEBUG_PRINT("Unable to locate '%s'.", filename);
  }
}

}  // namespace

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
      // FIXME: This will only ever launch into stage2 if anything about this
      // file path changes in the build system (like if the "./" was removed).
      // Although it means we'd be in stage 1 longer, it might be more reliable
      // to use libc vfs tools.
      libc::startup::ArgvParam("./userboot-stage2"),
  };
  FindAndRunRootElfExe(userboot_end, params[0].arg, params,
                       sizeof(params) / sizeof(params[0]));
}

#else

int main(int argc, char **argv) {
  // Userboot stage 2 should be nearly the exact same as userboot stage 1. The
  // main differences are:
  // (1) This is a bonafied ELF binary so it's more easily debuggable with tools
  //     like readelf and objdump.
  // (2) It *should* contain functional libc environment. It could be that we
  //     haven't fully implemented something here. *should* means that "in an
  //     ideal world, it will be supported here and not elsewhere".
  printf("=== Userboot Stage 2 ===\n");
  DEBUG_PRINT("argc: %d\n", argc);
  DEBUG_PRINT("argv: %p\n", argv);

  // Launch a shell!
  char *argv2[] = {NULL};
  execv("/bin/shell", argv2);
}

#endif  // __USERBOOT_STAGE1__
