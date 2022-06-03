#include <kernel/kmalloc.h>
#include <kernel/paging.h>
#include <libc/tests/malloc.h>
#include <libc/tests/test.h>
#include <stdio.h>
#include <stdlib.h>

namespace tests {

namespace {

class PagingTests : public ::libc::tests::TestFramework<PagingTests> {
 public:
  PagingTests() : TestFramework() {}

 protected:
  void Setup() override {
    current_page_dir_ = &paging::GetCurrentPageDirectory();
    ASSERT_TRUE(current_page_dir_->isKernelPageDir());

    num_free_vpages_ = current_page_dir_->getNumFreeVPages();
    ASSERT_NE(num_free_vpages_, 0);

    num_free_ppages_ = pmm::GetNumFree4MPages();
  }

  void Teardown() override {
    ASSERT_EQ(current_page_dir_, &paging::GetCurrentPageDirectory());
    ASSERT_TRUE(current_page_dir_->isKernelPageDir());
    ASSERT_EQ(num_free_vpages_, current_page_dir_->getNumFreeVPages());
    ASSERT_EQ(num_free_ppages_, pmm::GetNumFree4MPages());
  }

 private:
  paging::PageDirectory4M *current_page_dir_ = nullptr;
  size_t num_free_vpages_ = 0;
  size_t num_free_ppages_ = 0;
};

// Ensure two virtual addresses mapped to the same physical address can change
// each other.
void TestVirtualMapping(PagingTests &) {
  int32_t free_ppage = pmm::GetNextFreePage();
  ASSERT_GE(free_ppage, 0);
  pmm::SetPageUsed(static_cast<uint32_t>(free_ppage));
  uintptr_t paddr = static_cast<uint32_t>(free_ppage) * pmm::kPageSize4M;

  auto &pd = paging::GetCurrentPageDirectory();
  int32_t free_vpage1 = pd.getNextFreePage();
  ASSERT_GE(free_vpage1, 0);
  uintptr_t vaddr1 = static_cast<uint32_t>(free_vpage1) * pmm::kPageSize4M;
  pd.MapPage(vaddr1, paddr, /*flags=*/0);

  int32_t free_vpage2 = pd.getNextFreePage();
  ASSERT_GE(free_vpage2, 0);
  uintptr_t vaddr2 = static_cast<uint32_t>(free_vpage2) * pmm::kPageSize4M;
  ASSERT_NE(free_vpage1, free_vpage2);
  pd.MapPage(vaddr2, paddr, /*flags=*/0);

  // Now changes we make to vaddr1 should also change the values at vaddr2.
  uint32_t *arr1 = reinterpret_cast<uint32_t *>(vaddr1);
  uint32_t *arr2 = reinterpret_cast<uint32_t *>(vaddr2);
  for (size_t i = 0; i < 8; ++i) {
    arr1[i] = i;
    ASSERT_EQ(arr2[i], i);
  }

  pd.UnmapPage(vaddr1);
  pd.UnmapPage(vaddr2);
  pmm::SetPageFree(static_cast<uint32_t>(free_ppage));
}

}  // namespace

void RunKernelTests() {
  printf("Running kernel tests\n");

  ::libc::tests::RunAllMallocTests();

  PagingTests paging_tests;
  RUN_TESTF(paging_tests, TestVirtualMapping);

  printf("All kernel tests passed!\n");
}

}  // namespace tests
