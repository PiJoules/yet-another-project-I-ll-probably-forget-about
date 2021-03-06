#ifndef USERBOOT_INCLUDE_SYSCALLS_H_
#define USERBOOT_INCLUDE_SYSCALLS_H_

#define SYS_DebugWrite 0
#define SYS_ProcessKill 1
#define SYS_AllocPage 2
#define SYS_PageSize 3
#define SYS_ProcessCreate 4
#define SYS_MapPage 5
#define SYS_ProcessStart 6
#define SYS_UnmapPage 7
#define SYS_ProcessInfo 8
#define SYS_DebugRead 9
#define SYS_ProcessWait 10
#define SYS_ChannelCreate 11
#define SYS_HandleClose 12
#define SYS_ChannelRead 13
#define SYS_ChannelWrite 14
#define SYS_TransferHandle 15

// AllocPage flags.
#define ALLOC_ANON 0x1
#define ALLOC_CURRENT 0x2

// MapPage flags.
#define SWAP_OWNER 0x1
#define MAP_ANON 0x2

// Task info kinds.
#define PROC_CURRENT 0
#define PROC_PARENT 1
#define PROC_CHILDREN 2

// Task signals.
#define TASK_READY 0x1
#define TASK_RUNNING 0x2
#define TASK_TERMINATED 0x4

#ifndef ASM_FILE

#include <status.h>
#include <stdlib.h>

namespace syscall {

using handle_t = uint32_t;

void DebugWrite(const char *str, size_t size);
void ProcessKill(uint32_t retval);
kstatus_t AllocPage(uintptr_t &vaddr, handle_t proc_handle, uint32_t flags);
size_t PageSize();
kstatus_t ProcessCreate(handle_t &proc_handle);
kstatus_t MapPage(uintptr_t vaddr, handle_t other_proc, uintptr_t &other_vaddr,
                  uint32_t flags);
void ProcessStart(handle_t proc, uintptr_t entry, uint32_t arg = 0);
void UnmapPage(uintptr_t page_addr);
kstatus_t DebugRead(char &c);
kstatus_t ProcessWait(handle_t proc, uint32_t signals,
                      uint32_t &received_signal, uint32_t &signal_val);
kstatus_t ProcessInfo(handle_t proc, uint32_t kind, void *dst,
                      size_t buffer_size, size_t &written_or_needed);
void ChannelCreate(handle_t &end1, handle_t &end2);
void HandleClose(handle_t handle);
kstatus_t ChannelRead(handle_t endpoint, void *dst, size_t size,
                      size_t *bytes_available = nullptr);
void ChannelWrite(handle_t endpoint, const void *src, size_t size);
void TransferHandle(handle_t proc, handle_t handle);

// This is an RAII-style object for either allocating or mapping a page upon
// creation (in the current process), then unmapping it on destruction.
class PageAlloc {
 public:
  // Allocate an anonymous page in the current process. This is equivalent to:
  //
  //   ```
  //   uintptr_t addr_;
  //   AllocPage(addr_, /*proc_handle=*/0, ALLOC_ANON | ALLOC_CURRENT);
  //   ```
  //
  PageAlloc() {
    if (AllocPage(addr_, /*proc_handle=*/0, ALLOC_ANON | ALLOC_CURRENT) !=
        K_OK) {
      abort();
    }
  }

  ~PageAlloc() { UnmapPage(addr_); }

  uintptr_t getAddr() const { return addr_; }

  // Map a address in another process to the same physical page this
  // virtual page points to. This is equivalent to:
  //
  //   ```
  //   uintptr_t other_addr;
  //   MapPage(addr_, other_proc, other_addr, flags);
  //   ```
  //
  kstatus_t Map(handle_t other_proc, uintptr_t &other_addr,
                uint32_t flags = 0) const {
    return MapPage(addr_, other_proc, other_addr, flags);
  }

  kstatus_t MapAnonAndSwap(handle_t other_proc, uintptr_t &other_addr) const {
    return MapPage(addr_, other_proc, other_addr,
                   /*flags=*/SWAP_OWNER | MAP_ANON);
  }

 private:
  uintptr_t addr_;
};

// Perform a blocking read on a channel until we successfully read bytes
// off it.
inline void ChannelReadBlocking(handle_t channel, void *dst, size_t size) {
  kstatus_t status;
  do { status = ChannelRead(channel, dst, size); } while (status != K_OK);
}

}  // namespace syscall

#endif  // ASM_FILE

#endif  // USERBOOT_INCLUDE_SYSCALLS_H_
