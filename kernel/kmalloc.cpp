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

void AskForMoreKernelSpace(uintptr_t &alloc, size_t &alloc_size) {
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
                           /*alloc_size=*/pmm::kPageSize4M,
                           /*ask=*/AskForMoreKernelSpace);
}

}  // namespace kmalloc
