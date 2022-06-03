// Add this here because we add all libc sources via globbing. Rather than
// messing with that, we'll just wrap this in a guard.
#ifndef __KERNEL__

#include <assert.h>
#include <libc/malloc.h>
#include <libc/startup/globalstate.h>
#include <libc/startup/startparams.h>
#include <libc/startup/vfs.h>
#include <status.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscalls.h>

extern "C" int main(int argc, char **argv);

#ifndef __USERBOOT_STAGE1__

namespace {

libc::startup::GlobalState *gGlobalState = nullptr;
libc::startup::RootDir *gGlobalFs = nullptr;
libc::startup::Dir *gCurrentDir = nullptr;

}  // namespace

namespace libc {
namespace startup {
const Dir *GetCurrentDir() { return gCurrentDir; }
const RootDir *GetGlobalFS() { return gGlobalFs; }
const GlobalState *GetGlobalState() { return gGlobalState; }
}  // namespace startup
}  // namespace libc

#endif  // __USERBOOT_STAGE1__

// `arg` is the one argument that was passed to this process. When entering
// userboot stage 1, this argument is unused since the kernel doesn't pass us
// an argument. Otherwise (when entering either userboot stage 2 or some other
// regular ELF program), this contains a pointer to the `GlobalState` for this
// process which contains any information needed for a functional libc
// environment (`main` arguments, file system, working dir, etc).
extern "C" void __libc_start_main([[maybe_unused]] uint32_t arg) {
  // Allocate one page for `malloc` to use.
  uintptr_t malloc_page;
  kstatus_t status = syscall::AllocPage(malloc_page, /*proc_handle=*/0,
                                        ALLOC_ANON | ALLOC_CURRENT);
  if (status != K_OK) {
    printf("ERROR: UNABLE TO ALLOCATE A PAGE FOR MALLOC!!!\n");
    abort();
  }

  // This is a regression test for asserting that, prior to this binary being
  // loaded, we have correctly copied the instructions into the page this is
  // located on (ie. it's not all zeros from a recently allocated page).
  // Normally, this would result in a GOT relocation that would be patched by
  // the elf loaded, but for userboot stage 1, that GOT will not be patched and
  // stay filled with virtual offsets into the binary. This is still OK since
  // for userboot stage 1, this function should not be at the start of the
  // binary (`__user_program_entry` should).
  uintptr_t start_main_loc = reinterpret_cast<uintptr_t>(__libc_start_main);
  assert(start_main_loc);

  // TODO: Might also want to check if this page is at address zero.
  // Technically having a virtual page start at zero should be ok, but it might
  // be nicer to users if any heap allocations didn't start at zero. This
  // shouldn't ever cause `malloc` to return zero even if the page was at
  // zero since the first MallocHead would actually be at zero (meaning the
  // first possible pointer returned by `malloc` would actually return
  // whatever `sizeof(MallocHeader)` is).
  assert(malloc_page);
  size_t pagesize = syscall::PageSize();
  libc::malloc::Initialize(malloc_page, pagesize);

#ifdef __USERBOOT_STAGE1__
  // NOTE: argc and argv are meaningless here since we jumped directly from the
  // kernel, so we can ignore them here.
  main(/*argc=*/0, /*argv=*/nullptr);
#else
  assert(arg % alignof(libc::startup::GlobalState) == 0);
  gGlobalState = reinterpret_cast<libc::startup::GlobalState *>(arg);

  libc::startup::RootDir root;

  // FIXME: Technically, the vfs page we allocated could be at address zero, so
  // this would be incorrect. If we fixup userboot to just always set this to
  // some correct page, we won't have to check this here.
  if (gGlobalState->vfs_page) {
    libc::startup::InitVFS(root, gGlobalState->vfs_page);
    gGlobalFs = &root;
    gCurrentDir = &root;
  }

  // From here, we need to extract params from our process argument to
  // create argc and argv.
  int argc;
  char **argv;
  libc::startup::UnpackParams(gGlobalState->argv_page, argc, argv);

  if (!gGlobalFs) { printf("WARN: Virtual filesystem not available!\n"); }

  main(argc, argv);
#endif
}

#endif  // __KERNEL__