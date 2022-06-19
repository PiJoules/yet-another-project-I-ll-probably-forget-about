#include <kernel/kernel.h>
#include <kernel/kmalloc.h>
#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <libc/malloc.h>
#include <stdio.h>

#include <algorithm>

extern "C" uint32_t __KERNEL_BEGIN;

namespace kmalloc {
namespace {

// TODO: We cannot simply just pass this to the allocator initializer because
// this will involve adding another kernel page to the current address space,
// but other processes have no easy way of mapping this to their address space.
// The nice thing to have would be this mapped to the same virtual address for
// every address space (since that would mean the kernel can just reference
// this new page from any address space), but that's not easy because it would
// require each address space having the same free page, which isn't easy to
// guarantee.
//
// If we ever want to allocate a new page, another method could be switching to
// the main kernel task every time we enter a syscall, do any memory allocation
// there, then change back to the original task address space before exiting
// the syscall. If there are any instances where we need to copy into user
// memory from kernel memory (like which channels), we can do something along
// the lines of mapping an anonymous virtual page in the kernel task to the
// physical page backing the virtual page in the original task and do the copy
// there, then unmap this anonymous page and switch back to the original task
// address space before exiting the syscall. (I hope this will work lol.)
[[maybe_unused]] void AskForMoreKernelSpace(uintptr_t &alloc,
                                            size_t &alloc_size) {
  // Allocate some anonymous page.
  auto &pd = paging::GetKernelPageDirectory();
  // FIXME: Might want to move this hardcoded `1` into a setting somewhere.
  int32_t free_vpage = pd.getNextFreePage(/*lower_bound=*/1);
  assert(free_vpage >= 0 && "Out of virtual memory");
  uintptr_t page_vaddr = pmm::PageToAddr(static_cast<uint32_t>(free_vpage));

  int32_t free_ppage = pmm::GetNextFreePage();
  assert(free_ppage >= 0 && "Out of physical memory");
  pd.MapPage(page_vaddr, pmm::PageToAddr(static_cast<uint32_t>(free_ppage)),
             /*flags=*/0);
  pmm::SetPageUsed(static_cast<uint32_t>(free_ppage));

  // TODO: Should we also record the owned page? See comment in SYS_AllocPage.
  alloc = page_vaddr;
  alloc_size = pmm::kPageSize4M;
}

}  // namespace

void Initialize() {
  // For simplicity, we will place all kernel allocations on a single 4MB page.
  // This means we will not be able to allocate anything greater than 4MB and
  // if we hit the limit, we're done.
  //
  // Allocate just one 4MB page to place all kernel allocations in. We can get
  // the virtual page that just comes after our kernel.
  uintptr_t kernel_begin = reinterpret_cast<uintptr_t>(&__KERNEL_BEGIN);
  uintptr_t kmalloc_vaddr = kernel_begin + pmm::kPageSize4M;
  assert(!paging::GetCurrentPageDirectory().VaddrIsMapped(kmalloc_vaddr));

  int32_t free_ppage = pmm::GetNextFreePage();
  assert(free_ppage >= 0 && "No free physical pages?");
  assert(!pmm::PageIsUsed(static_cast<uint32_t>(free_ppage)));
  pmm::SetPageUsed(static_cast<uint32_t>(free_ppage));
  paging::GetCurrentPageDirectory().MapPage(
      kmalloc_vaddr, static_cast<uint32_t>(free_ppage) * pmm::kPageSize4M,
      /*flags=*/0);

  libc::malloc::Initialize(/*alloc_start=*/kmalloc_vaddr,
                           /*alloc_size=*/pmm::kPageSize4M);
}

}  // namespace kmalloc
