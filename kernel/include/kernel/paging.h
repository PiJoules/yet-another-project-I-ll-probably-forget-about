#ifndef KERNEL_INCLUDE_KERNEL_PAGING_H_
#define KERNEL_INCLUDE_KERNEL_PAGING_H_

#include <assert.h>
#include <kernel/isr.h>
#include <kernel/pmm.h>
#include <stdint.h>
#include <string.h>

#define PG_PRESENT 0x00000001  // page directory / table
#define PG_WRITE 0x00000002    // page is writable
#define PG_USER 0x00000004     // page can be accessed by user (et. all)
#define PG_PWT \
  0x00000008  // If the bit is set, write-through caching is enabled. If not,
              // then write-back is enabled instead.
#define PG_PCD \
  0x00000010  // If the bit is set, the page will not be cached. Otherwise, it
              // will be.
#define PG_4MB 0x00000080  // pages are 4MB
#define PG_GLOBAL \
  0x00000100  // Tells the processor not to invalidate the TLB entry
              // corresponding to the page upon a MOV to CR3 instruction. Bit 7
              // (PGE) in CR4 must be set to enable global pages.

namespace paging {

void Initialize();

constexpr size_t kPageDirAlignment =
    4096;  // Page directories must be 4KB aligned.

// Mask everything except the first 4MB.
constexpr const uintptr_t kPageMask4M = ~UINT32_C(0x3FFFFF);

class PageDirectory4M {
 public:
  uint32_t *get() { return pd_impl_; }
  const uint32_t *get() const { return pd_impl_; }

  // Map unmapped virtual memory to physical memory.
  //
  // The virtual and physical addresses must be page-aligned. The physical
  // memory may or may not be alread in use.
  void MapPage(uintptr_t v_addr, uintptr_t p_addr, uint8_t flags);

  void UnmapPage(uintptr_t vaddr);
  bool VaddrIsMapped(uintptr_t vaddr) const;
  bool isKernelPageDir() const;

  void Clear() { memset(pd_impl_, 0, sizeof(pd_impl_)); }
  size_t getNumFreeVPages() const;

  // Get the next free virtual page in this page directory. `lower_bound`
  // indicates the minimum page number we want to start looking from. This
  // can be set to 1 to indicate we want the first free page that doesn't
  // start at virtual address zero.
  //
  // Return negative number on no available virtual pages.
  int32_t getNextFreePage(uint32_t lower_bound = 0) const;

  // Works similar to memcpy, but it copies data from the `src` virtual
  // address in the *current* page directory into the `dst` virtual address
  // in *this* page directory. If the current page directory is this page
  // directory. This just performs a normal memcpy.
  void Memcpy(uintptr_t dst, uintptr_t src, size_t size);

  // Copy data from the address `src` in another page directory `other_pd`
  // into the address `dst` in this directory.
  void Memcpy(PageDirectory4M &other_pd, void *dst, const void *src,
              size_t size);

  // Get the physical address a virtual address is mapped to.
  uintptr_t getPhysicalAddr(uintptr_t vaddr) const;

  // Creates a copy of this page directory that contains the same page
  // mappings as the original. This is useful for ensuring that new page
  // directories cloned from the main kernel directory contain the same
  // mappings to kernel pages (such as the page the kernel is on and
  // wherever kernel allocations are stored).
  PageDirectory4M *Clone() const;

  void DumpMappedPages() const;

  PageDirectory4M() = default;

 private:
  PageDirectory4M(const PageDirectory4M &) = default;

  uint32_t &getPDE(uint32_t vaddr);
  const uint32_t &getPDE(uint32_t vaddr) const;

  alignas(kPageDirAlignment) uint32_t pd_impl_[pmm::kNumPageDirEntries];
};

PageDirectory4M &GetCurrentPageDirectory();
PageDirectory4M &GetKernelPageDirectory();
void SwitchPageDirectory(PageDirectory4M &pd);

void InspectPhysicalMem(uintptr_t pstart, uintptr_t pend);
void PageFaultHandler(isr::registers_t *regs);

}  // namespace paging

#endif  // KERNEL_INCLUDE_KERNEL_PAGING_H_
