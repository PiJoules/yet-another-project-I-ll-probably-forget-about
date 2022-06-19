#define LOCAL_DEBUG_LVL 0

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
#include <unistd.h>

extern "C" int main(int argc, char **argv);
extern "C" char **environ;

#ifndef __USERBOOT_STAGE1__

namespace {

libc::startup::RootDir *gGlobalFs = nullptr;
libc::startup::Dir *gCurrentDir = nullptr;
libc::startup::Envp *gEnvp = nullptr;
std::unique_ptr<char[]> *gPlainEnv = nullptr;
const void *gRawVfsData = nullptr;

void SetCurrentDir(const char *pwd) {
  libc::startup::VFSNode *node = libc::startup::GetNodeFromPath(pwd);
  if (!node) {
    printf("WARN: Unknown path '%s'. Not using this as CWD.\n", pwd);
    return;
  }

  if (!node->isDir()) {
    printf("WARN: Path '%s' is not a directory. Not using this as CWD.\n", pwd);
    return;
  }

  SetCurrentDir(static_cast<libc::startup::Dir *>(node));
}

constexpr char kPWDEnv[] = "PWD";
constexpr char kPATHEnv[] = "PATH";

}  // namespace

namespace libc {
namespace startup {
Dir *GetCurrentDir() { return gCurrentDir; }
RootDir *GetGlobalFS() { return gGlobalFs; }
Envp *GetEnvp() { return gEnvp; }
std::unique_ptr<char[]> *GetPlainEnv() { return gPlainEnv; }
const void *GetRawVfsData() { return gRawVfsData; }

void SetCurrentDir(Dir *wd) {
  assert(wd);
  gCurrentDir = wd;
  gEnvp->setVal(kPWDEnv, wd->getAbsPath());
}
}  // namespace startup
}  // namespace libc

#endif  // __USERBOOT_STAGE1__

// `arg` is the one argument that was passed to this process. When entering
// userboot stage 1, this argument is unused since the kernel doesn't pass us
// an argument. Otherwise (when entering either userboot stage 2 or some other
// regular ELF program), this contains a pointer to the `GlobalState` for this
// process which contains any information needed for a functional libc
// environment (`main` arguments, file system, working dir, etc).
extern "C" int __libc_start_main([[maybe_unused]] uint32_t arg) {
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
  return main(/*argc=*/0, /*argv=*/nullptr);
#else
  syscall::handle_t startup_channel = arg;

  libc::startup::RootDir root;

  // Each of these things we initialize follows a pattern of:
  // - First thing off the stream is the expected size.
  // - Second thing is some packed data of the size we just got that we can use
  //   to construct an object.
  size_t vfssize;
  syscall::ChannelReadBlocking(startup_channel, &vfssize, sizeof(vfssize));
  ResizableBuffer vfsbuffer(vfssize);
  syscall::ChannelReadBlocking(startup_channel, vfsbuffer.getData(), vfssize);
  libc::startup::InitVFS(root,
                         reinterpret_cast<uintptr_t>(vfsbuffer.getData()));
  gRawVfsData = vfsbuffer.getData();
  gGlobalFs = &root;
  gCurrentDir = &root;

  libc::startup::Envp envp;
  size_t envpsize;
  syscall::ChannelReadBlocking(startup_channel, &envpsize, sizeof(envpsize));
  ResizableBuffer buffer(envpsize);
  syscall::ChannelReadBlocking(startup_channel, buffer.getData(), envpsize);
  envp.Unpack(buffer);
  gEnvp = &envp;
  std::unique_ptr<char[]> plain_env = gEnvp->getPlainEnvp();
  gPlainEnv = &plain_env;
  environ = reinterpret_cast<char **>(gPlainEnv->get());

  // The current working dir points to the root dir, but if the `PWD` env
  // variable is set, then we can manually move to that dir.
  if (const char *pwd = getenv(kPWDEnv)) { SetCurrentDir(pwd); }

  // Set the `PATH` if it's not already added.
  std::string *path_env = gEnvp->getVal(kPATHEnv);
  if (!path_env) { gEnvp->setVal(kPATHEnv, "/bin"); }

  // From here, we need to extract params from our process argument to
  // create argc and argv.
  size_t argvsize;
  syscall::ChannelReadBlocking(startup_channel, &argvsize, sizeof(argvsize));
  buffer.Clear();
  buffer.Resize(argvsize);
  syscall::ChannelReadBlocking(startup_channel, buffer.getData(), argvsize);
  int argc;
  char **argv;
  libc::startup::UnpackParams(reinterpret_cast<uintptr_t>(buffer.getData()),
                              argc, argv);

  syscall::HandleClose(startup_channel);
  return main(argc, argv);
#endif
}
