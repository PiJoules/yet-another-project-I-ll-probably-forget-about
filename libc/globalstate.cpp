#ifndef __KERNEL__

#include <assert.h>
#include <libc/startup/globalstate.h>
#include <string.h>
#include <syscalls.h>

namespace libc {
namespace startup {

uintptr_t ApplyGlobalState(const GlobalState &state,
                           syscall::handle_t other_proc) {
  syscall::PageAlloc global_state_page;
  uintptr_t addr = global_state_page.getAddr();
  assert(addr % alignof(GlobalState) == 0);
  *reinterpret_cast<GlobalState *>(addr) = state;
  uintptr_t other_addr;
  global_state_page.MapAnonAndSwap(other_proc, other_addr);
  return other_addr;
}

uintptr_t ApplyVFSData(uintptr_t data_loc, size_t size,
                       syscall::handle_t other_proc) {
  syscall::PageAlloc ustar_page;
  uintptr_t addr = ustar_page.getAddr();
  memcpy(reinterpret_cast<void *>(addr), reinterpret_cast<void *>(data_loc),
         size);
  uintptr_t other_addr;
  ustar_page.MapAnonAndSwap(other_proc, other_addr);
  return other_addr;
}

}  // namespace startup
}  // namespace libc

#endif  // __KERNEL__
