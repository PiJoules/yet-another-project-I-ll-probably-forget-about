#include <libc/tests/malloc.h>

#include <algorithm>

namespace libc {
namespace tests {

void TestBasicMalloc(MallocTests &) {
  constexpr size_t kSizeMax = 1000;
  constexpr size_t kAlignBitsMax = 21;  // 2MB
  size_t avail = libc::malloc::GetAvailMemory();

  for (size_t size = 1; size <= kSizeMax; ++size) {
    for (size_t shift = 0; shift < kAlignBitsMax; ++shift) {
      void *ptr = libc::malloc::malloc(size, UINT32_C(1) << shift);
      ASSERT_NE(ptr, nullptr);
      libc::malloc::free(ptr);
      ASSERT_EQ(avail, libc::malloc::GetAvailMemory());
    }
  }
}

void TestMultipleMallocs(MallocTests &) {
  constexpr size_t kNumSizes = 7;
  size_t sizes[kNumSizes] = {1, 10, 100, 1000, 10000, 100000, 1000000};
  size_t *start = sizes;
  size_t *end = &sizes[kNumSizes - 1];
  size_t avail = malloc::GetAvailMemory();

  do {
    void *ptrs[kNumSizes];
    for (size_t i = 0; i < kNumSizes; ++i) {
      ptrs[i] = malloc::malloc(sizes[i]);
      ASSERT_NE(ptrs[i], nullptr);
    }
    for (size_t i = 0; i < kNumSizes; ++i) malloc::free(ptrs[i]);

    ASSERT_EQ(avail, malloc::GetAvailMemory());
  } while (std::next_permutation(start, end));
}

void RunAllMallocTests() {
  printf("Running malloc tests\n");

  MallocTests malloc_tests;
  RUN_TESTF(malloc_tests, TestBasicMalloc);
  RUN_TESTF(malloc_tests, TestMultipleMallocs);

  printf("All malloc tests passed!\n");
}

}  // namespace tests
}  // namespace libc
