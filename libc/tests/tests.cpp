#include <libc/tests/test.h>

namespace libc {
namespace tests {

template <>
void Print(const decltype(nullptr) &) {
  printf("nullptr");
}

template <>
void Print(const uint32_t &x) {
  printf("%u", x);
}

template <>
void Print(const int32_t &x) {
  printf("%d", x);
}

template <>
void Print(const bool &b) {
  printf("%s", b ? "true" : "false");
}

void RunTest(void (*test)(), const char *test_name) {
  printf("Running `%s` ... ", test_name);
  test();
  printf("Passed\n");
}

}  // namespace tests
}  // namespace libc
