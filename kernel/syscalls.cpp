#include <kernel/channel.h>
#include <kernel/isr.h>
#include <kernel/scheduler.h>
#include <kernel/serial.h>
#include <kernel/status.h>
#include <kernel/syscalls.h>
#include <stdio.h>
#include <stdlib.h>

// Set this to 1 to enable kernel traces.
#define LOCAL_KTRACE 0

#if LOCAL_KTRACE
#define KTRACE(...) printf("KTRACE> " __VA_ARGS__)
#else
#define KTRACE(...)
#endif

// Define this to be the lowest free virtual page we would like to pass to the
// user when anonymously allocating a page. Technically, we're allowed to pass
// the zero/first page, but for debugging purposes, it's probably easier just
// to ignore this for consideration.
#define FREE_PAGE_LOWER_BOUND 1

namespace syscalls {

namespace {

// Print a C-style string at the address specified by EBX.
void SYS_DebugWrite(isr::registers_t *regs) {
  const char *str = reinterpret_cast<const char *>(regs->ebx);
  printf("%s", str);
}

// Try to read a character from serial. This accepts arguments via the
// following registers:
//
//   EBX - The virtual address to store the character at (if successful).
//
// This sets return values via the following registers:
//
//   EAX - The result status of this syscall. (K_OK if a character was read.)
//
void SYS_DebugRead(isr::registers_t *regs) {
  char *dst = reinterpret_cast<char *>(regs->ebx);
  bool success = serial::TryRead(*dst);
  regs->eax = success ? K_OK : K_UNABLE_TO_READ;
}

// Stop a process and pass a return value. This accepts arguments via the
// following registers:
//
//   EBX - The return value. Tasks waiting on a TASK_TERMINATED signal will
//         received this value.
//
void SYS_ProcessKill(isr::registers_t *regs) {
  uint32_t retval = regs->ebx;
  KTRACE("killing %p with return value 0x%x\n", &scheduler::GetCurrentTask(),
         retval);
  scheduler::Schedule(/*regs=*/nullptr, retval);
  abort();
}

using handle_t = uint32_t;

enum alloc_page_flags_t : uint32_t {
  ALLOC_ANON = 0x1,
  ALLOC_CURRENT = 0x2,
};

// Allocate a physical page, map it somewhere in this address space, and return
// the virtual address it's mapped to. This accepts arguments via the following
// registers:
//
//   EBX - The virtual address to map this page to. If this address is already
//         mapped, then K_VPAGE_MAPPED is set as the status.
//   ECX - The handle for the process who's address space we want to map to.
//   EDX - Optional flags
//         ALLOC_ANON - Map to an anonymous memory address. If this is provided,
//                      then the address passed to EBX is ignored.
//         ALLOC_CURRENT - Map to the address space of the current process. If
//                         this is provided, then the proccess handle passed to
//                         ECX is ignored.
//
// This sets return values via the following registers:
//
//   EAX - The result status of this syscall.
//   EBX - The virtual address this page is mapped to.
//
void SYS_AllocPage(isr::registers_t *regs) {
  uintptr_t page_vaddr = regs->ebx;
  handle_t proc_handle = regs->ecx;
  uint32_t flags = regs->edx;

  int32_t free_ppage = pmm::GetNextFreePage();
  if (free_ppage < 0) {
    regs->eax = K_OOM_PHYS;
    return;
  }

  scheduler::Task *task;
  if (flags & ALLOC_CURRENT) {
    task = &scheduler::GetCurrentTask();
  } else {
    task = reinterpret_cast<scheduler::Task *>(proc_handle);
  }

  auto &pd = task->getPageDir();
  if (flags & ALLOC_ANON) {
    int32_t free_vpage =
        pd.getNextFreePage(/*lower_bound=*/FREE_PAGE_LOWER_BOUND);
    if (free_vpage < 0) {
      regs->eax = K_OOM_VIRT;
      return;
    }
    page_vaddr = pmm::PageToAddr(static_cast<uint32_t>(free_vpage));
  } else if (pd.VaddrIsMapped(page_vaddr)) {
    regs->eax = K_VPAGE_MAPPED;
    return;
  }

  pd.MapPage(page_vaddr, pmm::PageToAddr(static_cast<uint32_t>(free_ppage)),
             /*flags=*/PG_USER);

  regs->eax = K_OK;
  regs->ebx = page_vaddr;

  // TODO: This should also work for kernel tasks.
  assert(task->isUser());
  task->RecordOwnedPage(
      pmm::AddrToPage(task->getPageDir().getPhysicalAddr(page_vaddr)));
}

void SYS_PageSize(isr::registers_t *regs) { regs->eax = pmm::kPageSize4M; }

// Create (but do not start) a new process. The syscall sets return values via
// the follwing registers:
//
//   EAX - The result status of this syscall.
//   EBX - The handle to this process. This is only valid if the syscall result
//         is K_OK.
//
void SYS_ProcessCreate(isr::registers_t *regs) {
  paging::PageDirectory4M *user_pd = paging::GetKernelPageDirectory().Clone();
  auto *init_user_task = new scheduler::Task(/*user=*/true, *user_pd,
                                             &scheduler::GetCurrentTask());

  regs->ebx = reinterpret_cast<uint32_t>(init_user_task);
  regs->eax = K_OK;
}

// Map the page for a given virtual address in the current process to a virtual
// page in another process. That is, both virtual pages are mapped to the same
// physical page. Exactly one of these virtual pages must be already mapped to
// a physical page. This will only update the page table of the process that
// doesn't have the physical page backing. This will do nothing and return a
// K_OK if the handle for the other process and the current process are the
// same.
//
// This accepts arguments via the following registers:
//
//   EBX - The virtual address in the address space for this process we
//         want to map to.
//   ECX - The handle for the other process to map to.
//   EDX - The virtual address in the address space for the other process we
//         want to map to.
//   ESI - Flags
//         SWAP_OWNER - If provided, and these tasks are different, this swaps
//                      the owner of the physical page.
//         MAP_ANON - Map to a free page in the other process' address space.
//                    If this is provided, the value passed to EDX is ignored.
//
// This sets return values via the following registers:
//
//   EAX - The result status.
//   EBX - The virtual address in the other process we mapped to.
//
void SYS_MapPage(isr::registers_t *regs) {
  uintptr_t vaddr1 = regs->ebx;
  handle_t handle2 = regs->ecx;
  uintptr_t vaddr2 = regs->edx;
  uint32_t flags = regs->esi;

  auto *task1 = &scheduler::GetCurrentTask();
  auto *task2 = reinterpret_cast<scheduler::Task *>(handle2);
  if (task1 == task2) {
    regs->eax = K_OK;
    regs->ebx = vaddr2;
    return;
  }
  auto &pd1 = task1->getPageDir();
  auto &pd2 = task2->getPageDir();

  assert(&pd1 != &pd2);

  enum map_page_flags_t : uint32_t {
    SWAP_OWNER = 0x1,
    MAP_ANON = 0x2,
  };
  if (flags & MAP_ANON) {
    int32_t free_vpage =
        pd2.getNextFreePage(/*lower_bound=*/FREE_PAGE_LOWER_BOUND);
    if (free_vpage < 0) {
      regs->eax = K_OOM_VIRT;
      return;
    }
    vaddr2 = pmm::PageToAddr(static_cast<uint32_t>(free_vpage));
  }

  // NOTE: This means we don't accept virtual addresses that are on a page
  // boundary.
  if (vaddr1 % pmm::kPageSize4M || vaddr2 % pmm::kPageSize4M) {
    regs->eax = K_UNALIGNED_PAGE_ADDR;
    return;
  }

  // Exactly one of these must have a physical page backing it up.
  uintptr_t paddr, vaddr_to_map;
  paging::PageDirectory4M *dir_to_map;
  scheduler::Task *current_owner, *new_owner;
  if (pd1.VaddrIsMapped(vaddr1) && !pd2.VaddrIsMapped(vaddr2)) {
    paddr = pd1.getPhysicalAddr(vaddr1);
    dir_to_map = &pd2;
    vaddr_to_map = vaddr2;
    current_owner = task1;
    new_owner = task2;
  } else if (!pd1.VaddrIsMapped(vaddr1) && pd2.VaddrIsMapped(vaddr2)) {
    paddr = pd2.getPhysicalAddr(vaddr2);
    dir_to_map = &pd1;
    vaddr_to_map = vaddr1;
    current_owner = task2;
    new_owner = task1;
  } else {
    // Neither of these are mapped or both are mapped.
    regs->eax = K_VPAGE_MAPPED;
    return;
  }

  dir_to_map->MapPage(vaddr_to_map, paddr, /*flags=*/PG_USER);
  KTRACE("Mapped vaddr 0x%x => paddr 0x%x in task %p\n", vaddr_to_map, paddr,
         new_owner);

  if (flags & SWAP_OWNER) {
    uint32_t ppage = pmm::AddrToPage(paddr);
    current_owner->RemoveOwnedPage(ppage);
    new_owner->RecordOwnedPage(ppage);
  }

  regs->eax = K_OK;
  regs->ebx = vaddr2;
}

// Unmap a page from this page directory. If this is the owner of a physical
// page backing the virtual address, that physical page is freed.
//
// This accepts arguments via the following registers:
//
//   EBX - The virtual address in this address we want to unmap.
//
void SYS_UnmapPage(isr::registers_t *regs) {
  auto *task = &scheduler::GetCurrentTask();
  uintptr_t page_vaddr = regs->ebx;
  auto &pd = task->getPageDir();

  uint32_t ppage = pmm::AddrToPage(pd.getPhysicalAddr(page_vaddr));
  if (task->PageIsRecorded(ppage)) { task->RemoveOwnedPage(ppage); }

  pd.UnmapPage(page_vaddr);

  KTRACE("Unmapped vaddr 0x%x (paddr 0x%x) in task %p (owner: %d)\n",
         page_vaddr, pmm::PageToAddr(ppage), task, did_own);
}

// Start a process. This accepts arguments via the following registers:
//
//   EBX - The handle to the process to start.
//   ECX - The entry point for the new process.
//   EDX - An argument to be passed to the new process. EAX will be set to this
//         at the start of the new process.
//
void SYS_ProcessStart(isr::registers_t *regs) {
  handle_t proc_handle = regs->ebx;
  uintptr_t entry = regs->ecx;
  uint32_t arg = regs->edx;

  KTRACE("Starting task 0x%x at entry 0x%x with arg 0x%x\n", proc_handle, entry,
         arg);

  auto *task = reinterpret_cast<scheduler::Task *>(proc_handle);
  task->setEntry(entry);
  task->setArg(arg);
  RegisterTask(*task);
}

kstatus_t TryCopy(const void *src, size_t src_size, void *buff,
                  size_t buff_size) {
  if (buff_size < src_size) { return K_BUFFER_TOO_SMALL; }
  memcpy(buff, src, src_size);
  return K_OK;
}

// Retrieve information about a process.
//
// This accepts arguments via the following registers:
//
//   EBX - The handle to the process to retrieve info on.
//   ECX - Info kind
//         PROC_CURRENT - If provided, this ignores the value passed to EBX and
//                        retrieves info on the current process.
//         PROC_PARENT - Get the parent of the handle provided in EBX. If the
//                       parent is no longer running, this will be zero.
//         PROC_CHILDREN - Get an array of handles representing child tasks of
//                         the task provided in EBX. If there are no children,
//                         the returning status is still K_OK and zero bytes
//                         are written.
//   EDX - The buffer to write the result to.
//   ESI - The size of the buffer the result is written to.
//
// This sets return values via the following registers:
//
//   EAX - The return status of this syscall.
//   EBX - If the status of this syscall is K_OK, this returns the number of
//         bytes written to the buffer.
//         If the status of this syscall is K_BUFFER_TOO_SMALL (that is, this
//         buffer is not large enough to hold the result), this returns the
//         number of bytes needed in total to write to the buffer. In this
//         case, this value will always be non-zero.
//
void SYS_ProcessInfo(isr::registers_t *regs) {
  enum process_info_flags_t : uint32_t {
    PROC_CURRENT = 0,
    PROC_PARENT = 1,
    PROC_CHILDREN = 2,
  };

  handle_t proc_handle = regs->ebx;
  auto flags = static_cast<process_info_flags_t>(regs->ecx);
  void *buffer = reinterpret_cast<void *>(regs->edx);
  size_t size = regs->esi;

  scheduler::Task *task;
  if (flags == PROC_CURRENT) {
    task = reinterpret_cast<scheduler::Task *>(&scheduler::GetCurrentTask());
  } else {
    task = reinterpret_cast<scheduler::Task *>(proc_handle);
    if (!scheduler::IsRunningTask(task)) {
      regs->eax = K_INVALID_HANDLE;
      return;
    }
  }

  switch (flags) {
    case PROC_CURRENT: {
      handle_t this_task = reinterpret_cast<handle_t>(task);
      regs->eax = TryCopy(&this_task, sizeof(handle_t), buffer, size);
      regs->ebx = sizeof(handle_t);
      break;
    }
    case PROC_PARENT: {
      handle_t parent = reinterpret_cast<handle_t>(task->getParent());
      regs->eax = TryCopy(&parent, sizeof(handle_t), buffer, size);
      regs->ebx = sizeof(handle_t);
      break;
    }
    case PROC_CHILDREN: {
      std::vector<scheduler::Task *> children = task->getChildren();
      static_assert(sizeof(scheduler::Task *) == sizeof(handle_t));
      regs->eax = TryCopy(children.data(), children.size() * sizeof(handle_t),
                          buffer, size);
      regs->ebx = children.size() * sizeof(handle_t);
      break;
    }
    default:
      regs->eax = K_INVALID_ARG;
      return;
  }
}

// This is a blocking syscall that waits for a state on a process. If no signals
// are provided, nothing happens (essentially making this a no-op).
//
// This accepts arguments via the following registers:
//
//   EBX - The handle to the process to start.
//   ECX - The state to wait for. States include:
//         TASK_READY - This task is en queue to run but hasn't started yet.
//         TASK_RUNNING - This task has started running its code.
//         TASK_TERMINATED - This task is being killed.
//
// This sets return values via the following registers:
//
//   EAX - The return status of this syscall.
//   EBX - The signal that we received.
//   ECX - A return value associated with this signal.
//
void SYS_ProcessWait(isr::registers_t *regs) {
  handle_t proc_handle = regs->ebx;
  auto signals = static_cast<scheduler::Task::signal_t>(regs->ecx);
  if (!signals) {
    regs->eax = K_OK;
    return;
  }

  auto *proc = reinterpret_cast<scheduler::Task *>(proc_handle);
  if (!scheduler::IsRunningTask(proc)) {
    regs->eax = K_INVALID_HANDLE;
    return;
  }

  scheduler::GetCurrentTask().WaitOn(*proc, signals);
  regs->eax = K_OK;

  // The next time we enter back into this task, it will be when we schedule
  // back into it after a signal has been received. The signal value will be
  // set there.
  scheduler::Schedule(regs, /*retval=*/0);
  abort();
}

// Create a bi-directional channel through which two tasks can privately
// communicate. Both ends are owned by the current process. Once a process
// stops, any endpoints of a channel it has are closed. If one end of a channel
// is closed but another is open, data can still be read from the open end,
// but it cannot be written to.
//
// This sets return values via the following registers:
//
//   EAX - The handle to one end of the channel.
//   EBX - The endpoint to the other end of the channel.
//
void SYS_ChannelCreate(isr::registers_t *regs) {
  channel::Endpoint *end1, *end2;
  channel::Create(end1, end2);
  regs->eax = reinterpret_cast<handle_t>(end1);
  regs->ebx = reinterpret_cast<handle_t>(end2);
}

// Close a handle. This has different behavior depending on the handle type. If
// this a handle to one endpoint of a channel, this will close the other
// endpoint also.
//
// This accepts arguments via the following syscalls:
//
//   EBX - The handle to close.
//
void SYS_HandleClose(isr::registers_t *regs) {
  handle_t handle = regs->ebx;

  // FIXME: This just assumes a channel endpoint handle.
  auto *endpoint = reinterpret_cast<channel::Endpoint *>(handle);
  endpoint->Close();
}

// Read from a channel.
//
// This accepts arguments via the following syscalls:
//
//   EBX - The handle to one end of the channel.
//   ECX - The destination address to write to.
//   EDX - The number of bytes to read.
//
// This sets return values via the following registers:
//
//   EAX - The return status. This is K_BUFFER_TOO_SMALL if the read size is
//         less than the number of bytes on the channel.
//   EBX - The number of bytes available to read. This is only set if the
//         number of bytes requested to read is greater than the number of
//         bytes in the channel.
//
void SYS_ChannelRead(isr::registers_t *regs) {
  auto *endpoint = reinterpret_cast<channel::Endpoint *>(regs->ebx);
  void *dst = reinterpret_cast<void *>(regs->ecx);
  size_t size = regs->edx;
  size_t bytes_available;
  bool success = endpoint->Read(dst, size, &bytes_available);
  if (success) {
    regs->eax = K_OK;
  } else {
    regs->eax = K_BUFFER_TOO_SMALL;
    regs->ebx = bytes_available;
  }
}

// Write to a channel.
//
// This accepts arguments via the following syscalls:
//
//   EBX - The handle to one end of the channel.
//   ECX - The source address to read data from.
//   EDX - The number of bytes to write.
//
void SYS_ChannelWrite(isr::registers_t *regs) {
  auto *endpoint = reinterpret_cast<channel::Endpoint *>(regs->ebx);
  void *src = reinterpret_cast<void *>(regs->ecx);
  size_t size = regs->edx;
  endpoint->Write(src, size);
}

// Transfer ownership of a handle to another process.
//
// This accepts arguments via the following registers:
//
//   EBX - The handle of the process to transfer ownership to.
//   ECX - The handle to transfer ownership.
//
void SYS_TransferHandle(isr::registers_t *regs) {
  handle_t proc_handle = regs->ebx;
  handle_t transfer_handle = regs->ecx;
  auto *task = reinterpret_cast<scheduler::Task *>(proc_handle);
  auto *endpoint = reinterpret_cast<channel::Endpoint *>(transfer_handle);

  // FIXME: This only works for channel handles.
  endpoint->TransferOwner(task);
}

constexpr isr::handler_t kSyscallHandlers[] = {
    SYS_DebugWrite,    SYS_ProcessKill, SYS_AllocPage,    SYS_PageSize,
    SYS_ProcessCreate, SYS_MapPage,     SYS_ProcessStart, SYS_UnmapPage,
    SYS_ProcessInfo,   SYS_DebugRead,   SYS_ProcessWait,  SYS_ChannelCreate,
    SYS_HandleClose,   SYS_ChannelRead, SYS_ChannelWrite, SYS_TransferHandle,
};
constexpr size_t kNumSyscalls =
    sizeof(kSyscallHandlers) / sizeof(isr::handler_t);

}  // namespace

void SyscallHandler(isr::registers_t *regs) {
  if (regs->eax < kNumSyscalls && kSyscallHandlers[regs->eax]) {
    isr::handler_t handler = kSyscallHandlers[regs->eax];
    handler(regs);
  } else {
    printf("unknown syscall %d\n", regs->eax);
    abort();  // TODO: Handle gracefully.
  }
}

}  // namespace syscalls
