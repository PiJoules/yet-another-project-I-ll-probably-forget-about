#include <assert.h>
#include <kernel/isr.h>
#include <kernel/kernel.h>
#include <kernel/multiboot.h>
#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <kernel/scheduler.h>
#include <kernel/serial.h>
#include <kernel/stacktrace.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" uint32_t __KERNEL_BEGIN, __KERNEL_END;

namespace paging {

void InspectPhysicalMem(uintptr_t start, uintptr_t end) {
  assert(start <= end);
  uintptr_t page_start = pmm::PageAddress(start);
  uintptr_t page_end = pmm::PageAddress(end);
  assert(page_start == page_end);

  auto &pd = GetCurrentPageDirectory();
  int32_t free_vpage = pd.getNextFreePage();
  assert(free_vpage >= 0);
  pd.MapPage((uint32_t)free_vpage, page_start, /*flags=*/0);

  const uint32_t *begin =
      (uint32_t *)(start - page_start + (uint32_t)free_vpage);
  const uint32_t *end_ = (uint32_t *)(end - page_end + (uint32_t)free_vpage);
  size_t i = 0;
  while (begin < end_) {
    printf("0x%x[%d]: 0x%x\n",
           (uintptr_t)begin - (uint32_t)free_vpage + page_start, i, *begin);
    ++i;
    ++begin;
  }

  pd.UnmapPage((uint32_t)free_vpage);
}

namespace {

PageDirectory4M gKernelPageDir;
PageDirectory4M *gCurrentPageDir;

void MapKernelPage(PageDirectory4M &pd) {
  uintptr_t kernel_start = reinterpret_cast<uintptr_t>(&__KERNEL_BEGIN);
  pd.MapPage(kernel_start, kernel_start, PG_PRESENT | PG_WRITE | PG_4MB);
}

}  // namespace

void PageFaultHandler(isr::registers_t *regs) {
  uint32_t faulting_addr;
  asm volatile("mov %%cr2, %0" : "=r"(faulting_addr));

  int present = regs->err_code & 0x1;
  int rw = regs->err_code & 0x2;
  int us = regs->err_code & 0x4;
  int reserved = regs->err_code & 0x8;
  int id = regs->err_code & 0x10;

  printf("Page fault!!! When trying to %s 0x%x \n- IP:0x%x\n",
         rw ? "write to" : "read from", faulting_addr, regs->eip);
  printf("- The page was %s\n", present ? "present" : "not present");

  if (reserved) printf("- Reserved bit was set\n");

  if (id) printf("- Caused by an instruction fetch\n");

  printf("- CPU was in %s\n", us ? "user-mode" : "supervisor mode");

  if (&scheduler::GetCurrentTask() == &scheduler::GetMainKernelTask()) {
    printf("- Occurred in main kernel task.\n");
  } else {
    printf("- Occurred in task at %p\n", &scheduler::GetCurrentTask());
  }

  regs->Dump();

  PageDirectory4M &pd = scheduler::GetCurrentTask().getPageDir();
  pd.DumpMappedPages();

  abort();
}

void PageDirectory4M::DumpMappedPages() const {
  printf("Mapped pages:\n");
  for (size_t i = 0; i < pmm::kNumPageDirEntries; ++i) {
    uint32_t pde = get()[i];
    if (pde) {
      printf("%u) 0x%x (vaddr 0x%x => paddr 0x%x, %s, %s, %s, %s, %s)\n", i,
             pde, pmm::PageToAddr(i), pmm::PageAddress(pde),
             pde & PG_PRESENT ? "present" : "not present",
             pde & PG_WRITE ? "writable" : "read-only",
             pde & PG_USER ? "user-accessible" : "user-inaccessible",
             pde & PG_PCD ? "cached" : "not cached",
             pde & PG_GLOBAL ? "global" : "not global");
    }
  }
}

void SwitchPageDirectory(PageDirectory4M &pd) {
  gCurrentPageDir = &pd;
  asm volatile("mov %0, %%cr3" ::"r"(pd.get()));
}

bool PageDirectory4M::isKernelPageDir() const {
  return this == gCurrentPageDir;
}

PageDirectory4M &GetCurrentPageDirectory() { return *gCurrentPageDir; }
PageDirectory4M &GetKernelPageDirectory() { return gKernelPageDir; }

void Initialize() {
  // Initialize and set the kernel page directory.
  gKernelPageDir.Clear();

  // Pages reserved for the kernel (4MB - 12MB). These are identity-mapped. This
  // means that changes to these virtual addresses will also update changes to
  // that same physical address. If other page directories clone this and use
  // the exact same vaddr-paddr mapping, then data written to memory on these
  // pages will update across all page directories.
  //
  // For the kernel page, this means all kernel data across all address spaces
  // and processes will be updated simultaniously.
  //
  // For the page directory region, this means that all address spaces have
  // knowledge of the page directory mappings of all other page directories.
  uintptr_t kernel_start = reinterpret_cast<uintptr_t>(&__KERNEL_BEGIN);
  uintptr_t kernel_end = reinterpret_cast<uintptr_t>(&__KERNEL_END);
  assert(kernel_end - kernel_start <= pmm::kPageSize4M &&
         "Kernel does not fit within a 4MB page");
  assert(kernel_start % pmm::kPageSize4M == 0 &&
         "Expected kernel start to be page-aligned.");
  assert(pmm::PageIsUsed(kernel_start / pmm::kPageSize4M) &&
         "Expected the kernel to already be mapped.");
  MapKernelPage(gKernelPageDir);

  SwitchPageDirectory(gKernelPageDir);

  // Enable paging.
  // PSE is required for 4MB pages.
  constexpr uint32_t kPagingFlag = 0x80000000;  // CR0 - bit 31
  constexpr uint32_t kPseFlag = 0x00000010;     // CR4 - bit 4
  asm volatile(
      "mov %%cr4, %%eax \n\
      or %1, %%eax \n\
      mov %%eax, %%cr4 \n\
      mov %%cr0, %%eax \n\
      or %0, %%eax \n\
      mov %%eax, %%cr0" ::"i"(kPagingFlag),
      "i"(kPseFlag));

  uint32_t cr0, cr4;
  asm volatile("mov %%cr0, %0" : "=r"(cr0));
  asm volatile("mov %%cr4, %0" : "=r"(cr4));
  assert(cr4 == kPseFlag && "Expected only Page Size Extension to be on.");
  // FIXME: Move this into a constant variable and document the flags.
  assert(cr0 == 0x80000011);
}

uint32_t &PageDirectory4M::getPDE(uint32_t vaddr) {
  assert(vaddr % pmm::kPageSize4M == 0 &&
         "Attempting to map a virtual address that is not 4MB aligned");
  return pd_impl_[vaddr / pmm::kPageSize4M];
}

const uint32_t &PageDirectory4M::getPDE(uint32_t vaddr) const {
  assert(vaddr % pmm::kPageSize4M == 0 &&
         "Attempting to map a virtual address that is not 4MB aligned");
  return pd_impl_[vaddr / pmm::kPageSize4M];
}

void PageDirectory4M::MapPage(uintptr_t vaddr, uintptr_t paddr, uint8_t flags) {
  DisableInterruptsRAII disable_interrupts_raii;

  // With 4MB pages, bits 31 through 12 are reserved, so the the physical
  // address must be 4MB aligned.
  assert(paddr % pmm::kPageSize4M == 0 &&
         "Attempting to map a page that is not 4MB aligned!");

  uint32_t &pde = getPDE(vaddr);
  assert(
      !(pde & PG_PRESENT) &&
      "The page directory entry for this virtual address is already assigned.");

  pde = paddr | (PG_PRESENT | PG_4MB | PG_WRITE | flags);
  assert(!(pde & PG_GLOBAL) && "DO NOT ENABLE THE GLOBAL BIT");

  // Invalidate page in TLB.
  asm volatile("invlpg %0" ::"m"(vaddr));
}

void PageDirectory4M::UnmapPage(uintptr_t vaddr) {
  DisableInterruptsRAII disable_interrupts_raii;

  uint32_t &pde = getPDE(vaddr);
  assert(
      (pde & PG_PRESENT) &&
      "The page directory entry for this virtual address is already assigned.");

  pde = 0;

  // Invalidate page in TLB.
  asm volatile("invlpg %0" ::"m"(vaddr));
}

bool PageDirectory4M::VaddrIsMapped(uintptr_t vaddr) const {
  return getPDE(vaddr) & PG_PRESENT;
}

size_t PageDirectory4M::getNumFreeVPages() const {
  size_t n = 0;
  for (size_t i = 0; i < pmm::kNumPageDirEntries; ++i)
    n += !bool(pd_impl_[i] & PG_PRESENT);
  return n;
}

int32_t PageDirectory4M::getNextFreePage(uint32_t lower_bound) const {
  for (size_t i = lower_bound; i < pmm::kNumPageDirEntries; ++i)
    if (!(pd_impl_[i] & PG_PRESENT)) return static_cast<int32_t>(i);
  return -1;
}

PageDirectory4M *PageDirectory4M::Clone() const {
  auto *pd = new PageDirectory4M(*this);
  assert(pd);
  return pd;
}

uintptr_t PageDirectory4M::getPhysicalAddr(uintptr_t vaddr) const {
  const uint32_t &pd_entry = getPDE(vaddr);
  assert((pd_entry & PG_PRESENT) && "Page for virtual address not present");
  return pd_entry & kPageMask4M;
}

void PageDirectory4M::Memcpy(uintptr_t dst, uintptr_t src, size_t size) {
  if (this == &GetCurrentPageDirectory()) {
    memcpy(reinterpret_cast<void *>(dst), reinterpret_cast<void *>(src), size);
    return;
  }

  // We are copying something from the current page directoy into another
  // page directory. We can do this by
  // 1) Getting the physical page corresponding to the virtual page where we
  //    want to copy to in the other (this) page directory.
  // 2) Finding a free virtual page in the current page directory and mapping
  //    that free vpage to the ppage from (1).
  // 3) Doing a memcpy from the `src` in the current page directory to the
  //    newly mapped vpage from (2).
  uintptr_t dst_paddr = getPhysicalAddr(pmm::PageAddress(dst));
  int32_t free_vpage = GetCurrentPageDirectory().getNextFreePage();
  assert(free_vpage >= 0);
  uintptr_t dst_page_vaddr = pmm::PageToAddr(static_cast<uint32_t>(free_vpage));
  GetCurrentPageDirectory().MapPage(dst_page_vaddr, dst_paddr, /*flags=*/0);
  size_t dst_offset = dst - pmm::PageAddress(dst);
  memcpy(reinterpret_cast<void *>(dst_page_vaddr + dst_offset),
         reinterpret_cast<void *>(src), size);

  // Finally unmap the temporary page.
  GetCurrentPageDirectory().UnmapPage(dst_page_vaddr);
}

}  // namespace paging
