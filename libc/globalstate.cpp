#ifndef __KERNEL__

#include <assert.h>
#include <libc/startup/globalstate.h>
#include <stdio.h>
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

// The start of the page will be the table of pointers to strings elsewhere in
// the page.
uintptr_t ApplyEnvp(const Envp &envp, syscall::handle_t other_proc) {
  syscall::PageAlloc envp_page;
  uintptr_t addr = envp_page.getAddr();

  uintptr_t other_addr;
  envp_page.MapAnonAndSwap(other_proc, other_addr);

  envp.ApplyRelative(
      reinterpret_cast<char *>(addr),
      reinterpret_cast<char *>(other_addr) - reinterpret_cast<char *>(addr));

  return other_addr;
}

void UnpackEnvp(char *const envp[], Envp &envp_vec) {
  while (*envp) {
    // Environment variables are in the format `KEY=VAL`.
    std::string entry(*envp);
    auto found_sep = entry.find('=');
    if (found_sep == std::string::npos || found_sep == 0) {
      printf("WARN: envp entry '%s' not in the format 'KEY=VAL'\n",
             entry.c_str());
      continue;
    }

    envp_vec.setVal(entry.substr(0, found_sep), entry.substr(found_sep + 1));

    ++envp;
  }
}

}  // namespace startup
}  // namespace libc

#endif  // __KERNEL__
