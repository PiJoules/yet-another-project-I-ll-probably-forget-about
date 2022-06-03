#include <assert.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <stdio.h>
#include <string.h>

extern "C" {

extern uint32_t __KERNEL_BEGIN;
extern uint32_t __KERNEL_END;

}  // extern "C"

namespace pmm {

namespace {

size_t gNum4MPages;

// This refers to the number of physical pages available. Each bit in this
// array represents an available page (directory) for use. A 32-bit address
// space means there can be up to 4GB of physical memory available, but that
// might not always be the case. `gNum4MPages` represents the actual number
// of pages available and may be smaller than `kNumPageDirEntries`.
uint8_t gPhysicalBitmap[kNumPageDirEntries / CHAR_BIT];

}  // namespace

size_t GetNum4MPages() { return gNum4MPages; }

bool PageIsUsed(uint32_t page) {
  assert(page <= kNumPageDirEntries);
  return gPhysicalBitmap[page / CHAR_BIT] & (1 << (page % CHAR_BIT));
}

void SetPageUsed(uint32_t page) {
  assert(!PageIsUsed(page));
  gPhysicalBitmap[page / CHAR_BIT] |= (1 << (page % CHAR_BIT));
}

void SetPageFree(uint32_t page) {
  assert(PageIsUsed(page));
  gPhysicalBitmap[page / CHAR_BIT] &= ~(1 << (page % CHAR_BIT));
}

void Dump() {
  printf("Physical 4M pages:\n");
  for (size_t i = 0; i < gNum4MPages; i += CHAR_BIT) {
    printf("  0x%x\n", gPhysicalBitmap[i / CHAR_BIT]);
  }
}

size_t GetNumFree4MPages() {
  auto num_zero_bits = [](uint8_t x) {
    size_t n = 0;
    x = ~x;  // Count 0s by bitwise NOTing and counting the 1s.
    while (x) {
      n += (x & 1);
      x >>= 1;
    }
    return n;
  };

  size_t num = 0;
  for (size_t i = 0; i < gNum4MPages / CHAR_BIT; ++i)
    num += num_zero_bits(gPhysicalBitmap[i]);

  if (gNum4MPages % CHAR_BIT)
    num += num_zero_bits(gPhysicalBitmap[gNum4MPages / CHAR_BIT]);

  return num;
}

size_t GetNumUsed4MPages() {
  size_t available = GetNumFree4MPages();
  assert(available <= gNum4MPages);
  return gNum4MPages - available;
}

void Initialize(uintptr_t mem_upper) {
  // The total memory avaiable can be provided by the upper bound value
  // provided by multiboot. This value is given in KB.
  uint32_t total_mem = RoundUp<kPageSize4M>(mem_upper * 1024);
  gNum4MPages = total_mem / pmm::kPageSize4M;

  assert(gNum4MPages && gNum4MPages <= kNumPageDirEntries &&
         "Unexpected number of 4MB pages.");

  if (gNum4MPages == kNumPageDirEntries) {
    // We're using the whole address space. Clear everything.
    memset(gPhysicalBitmap, 0, sizeof(gPhysicalBitmap));
  } else {
    // Intially mark everything as used.
    memset(gPhysicalBitmap, 0xFF, sizeof(gPhysicalBitmap));

    // Clear bottom of the bitmap.
    memset(gPhysicalBitmap, 0, gNum4MPages / CHAR_BIT + 1);
    gPhysicalBitmap[gNum4MPages / CHAR_BIT] =
        static_cast<uint8_t>(~((UINT8_C(1) << (gNum4MPages % CHAR_BIT)) - 1));
  }

  assert(GetNumFree4MPages() == gNum4MPages &&
         "Expected all pages to be available.");

  // uintptr_t kernel_begin = reinterpret_cast<uintptr_t>(&__KERNEL_BEGIN);
  // uintptr_t kernel_end = reinterpret_cast<uintptr_t>(&__KERNEL_END);
  // assert(kernel_begin % kPageSize4M == 0 &&
  //        "Expected the kernel to start on a page boundary.");

  // size_t kernel_size = kernel_end - kernel_begin;
  // assert(kernel_size <= kPageSize4M && "Expected the kernel to fit in a
  // page");

  //// Mark the page the kernel is on as the only used page.
  // SetPageUsed(kernel_begin / kPageSize4M);
  // assert(GetNumFree4MPages() == gNum4MPages - 1 &&
  //        "Expected the kernel to be the only used page.");
}

int32_t GetNextFreePage() {
  for (size_t i = 0; i < gNum4MPages / CHAR_BIT; ++i) {
    uint8_t x = gPhysicalBitmap[i];
    if (x == 0xFF) continue;  // All used up here.

    int idx = 0;
    while (x & 1) {
      ++idx;
      x >>= 1;
    }
    return static_cast<int32_t>(i) * CHAR_BIT + idx;
  }
  return -1;
}

}  // namespace pmm
