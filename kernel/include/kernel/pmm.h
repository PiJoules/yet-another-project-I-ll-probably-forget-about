#ifndef KERNEL_INCLUDE_KERNEL_PMM_H_
#define KERNEL_INCLUDE_KERNEL_PMM_H_

#include <stdint.h>

namespace pmm {

constexpr size_t kPageSize4M = 0x400000;  // 4MB
constexpr size_t kNumPageDirEntries =
    1024;  // If we support 4MB pages, and we have a virtual address space of
           // 4GB, then we have 1024 page directories we can allocate.

void Initialize(uintptr_t mem_upper);
size_t GetNum4MPages();
bool PageIsUsed(uint32_t page);
void SetPageUsed(uint32_t page);
void SetPageFree(uint32_t page);
size_t GetNumFree4MPages();
size_t GetNumUsed4MPages();
constexpr inline uint32_t AddrToPage(uintptr_t addr) {
  return addr / kPageSize4M;
}
constexpr inline uint64_t AddrToPage(uint64_t addr) {
  return addr / kPageSize4M;
}
constexpr inline uintptr_t PageToAddr(uint32_t page) {
  return page * kPageSize4M;
}

// Get the address of a page that this address is on.
constexpr inline uintptr_t PageAddress(uintptr_t addr) {
  return PageToAddr(AddrToPage(addr));
}

// Return the page number of the next free physical page. Return a negative
// value if there are no free pages.
int32_t GetNextFreePage();

void Dump();

}  // namespace pmm

#endif  // KERNEL_INCLUDE_KERNEL_PMM_H_
