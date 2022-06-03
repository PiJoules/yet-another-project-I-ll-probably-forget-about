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

void SYS_ProcessKill(isr::registers_t *) {
  KTRACE("killing %p\n", &scheduler::GetCurrentTask());
  scheduler::Schedule(/*regs=*/nullptr);
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

// Retrieve information about a process. This accepts arguments via the
// following registers:
//
//   EBX - The handle to the process to retrieve info on.
//   ECX - Flags
//         CURRENT_PROC - If provided, this ignores the value passed to EBX and
//                        retrieves info on the current process.
//   EDX - The buffer to write the result to.
//   ESI - The size of the buffer the result is written to.
//
// This sets return values via the following registers:
//
//   EAX - The return status of this syscall.
//
void SYS_ProcessInfo(isr::registers_t *regs) {
  handle_t proc_handle = regs->ebx;
  enum process_info_flags_t : uint32_t {
    CURRENT_PROC = 0x1,
  };
  auto flags = static_cast<process_info_flags_t>(regs->ecx);
  void *buffer = reinterpret_cast<void *>(regs->edx);
  size_t size = regs->esi;

  if (flags & CURRENT_PROC) {
    proc_handle = reinterpret_cast<handle_t>(&scheduler::GetCurrentTask());
  } else if (!scheduler::IsRunningTask(
                 reinterpret_cast<scheduler::Task *>(proc_handle))) {
    regs->eax = K_INVALID_HANDLE;
    return;
  }
  auto *task = reinterpret_cast<scheduler::Task *>(proc_handle);

  struct proc_info_t {
    // The handle for the process specified.
    handle_t handle;

    // Parent process handle. Note that this will still return the parent
    // handle even if the parent task has exited.
    handle_t parent;
  };
  static_assert(sizeof(proc_info_t) == 8, "proc_info_t size changed!");

  if (size < sizeof(proc_info_t)) {
    regs->eax = K_BUFFER_TOO_SMALL;
    return;
  }

  proc_info_t info{
      .handle = reinterpret_cast<handle_t>(task),
      .parent = reinterpret_cast<handle_t>(task->getParent()),
  };
  memcpy(buffer, &info, sizeof(proc_info_t));

  regs->eax = K_OK;
}

constexpr isr::handler_t kSyscallHandlers[] = {
    SYS_DebugWrite,    SYS_ProcessKill, SYS_AllocPage,    SYS_PageSize,
    SYS_ProcessCreate, SYS_MapPage,     SYS_ProcessStart, SYS_UnmapPage,
    SYS_ProcessInfo,   SYS_DebugRead,
};
constexpr size_t kNumSyscalls =
    sizeof(kSyscallHandlers) / sizeof(isr::handler_t);

void SyscallHandler(isr::registers_t *regs) {
  if (regs->eax < kNumSyscalls && kSyscallHandlers[regs->eax]) {
    isr::handler_t handler = kSyscallHandlers[regs->eax];
    handler(regs);
  } else {
    printf("unknown syscall %d\n", regs->eax);
    abort();  // TODO: Handle gracefully.
  }
}

}  // namespace

void Initialize() { isr::RegisterHandler(kSyscallHandler, SyscallHandler); }

}  // namespace syscalls