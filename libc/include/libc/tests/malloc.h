#ifndef LIBC_INCLUDE_LIBC_TESTS_MALLOC_H_
#define LIBC_INCLUDE_LIBC_TESTS_MALLOC_H_

#include <libc/malloc.h>
#include <libc/tests/test.h>

namespace libc {
namespace tests {

class MallocTests : public libc::tests::TestFramework<MallocTests> {
 public:
  MallocTests() : TestFramework() {}

 protected:
  void Setup() override {
    starting_avail_mem_ = libc::malloc::GetAvailMemory();
  }

  void Teardown() override {
    ASSERT_EQ(starting_avail_mem_, libc::malloc::GetAvailMemory());
  }

 private:
  size_t starting_avail_mem_;
};

void TestBasicMalloc(MallocTests &);
void TestMultipleMallocs(MallocTests &);

// Simple function that any user linking against this libc can call to run any
// tests associated with this malloc implementation. Just call like:
//
// ```
// libc::tests::RunAllMallocTests();
// ```
//
void RunAllMallocTests();

}  // namespace tests
}  // namespace libc

#endif  // LIBC_INCLUDE_LIBC_TESTS_MALLOC_H_
