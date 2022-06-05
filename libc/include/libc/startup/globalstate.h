#ifndef LIBC_INCLUDE_LIBC_STARTUP_GLOBALSTATE_H_
#define LIBC_INCLUDE_LIBC_STARTUP_GLOBALSTATE_H_

#ifndef __KERNEL__

#include <libc/startup/vfs.h>
#include <stddef.h>
#include <syscalls.h>

namespace libc {
namespace startup {

// This is unique per-process.
struct GlobalState {
  uintptr_t argv_page;
  uintptr_t vfs_page;
};

Dir *GetCurrentDir();
RootDir *GetGlobalFS();
GlobalState *GetGlobalState();

// Create a page containing the `GlobalState` which is owned by another
// process. Return the virtual address in that new process this page can be
// accessed.
uintptr_t ApplyGlobalState(const GlobalState &state,
                           syscall::handle_t other_proc);

// Create a page containing data that will be unpacked from the `vfs_page`
// provided with the global state.
uintptr_t ApplyVFSData(uintptr_t data_loc, size_t size,
                       syscall::handle_t other_proc);

}  // namespace startup
}  // namespace libc

#endif  // __KERNEL__

#endif  // LIBC_INCLUDE_LIBC_STARTUP_GLOBALSTATE_H_
