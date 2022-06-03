#ifndef LIBC_INCLUDE_LIBC_TESTS_TEST_H_
#define LIBC_INCLUDE_LIBC_TESTS_TEST_H_

#include <stdio.h>
#include <stdlib.h>

#ifndef STR
#define __STR(s) STR(s)
#define STR(s) #s
#endif

#define ASSERT_EQ(val1, val2)                                           \
  {                                                                     \
    ::libc::tests::AssertEq(val1, val2, STR(val1), STR(val2), __FILE__, \
                            __LINE__);                                  \
  }

#define ASSERT_NE(val1, val2)                                           \
  {                                                                     \
    ::libc::tests::AssertNe(val1, val2, STR(val1), STR(val2), __FILE__, \
                            __LINE__);                                  \
  }

#define ASSERT_GE(val1, val2)                                           \
  {                                                                     \
    ::libc::tests::AssertGe(val1, val2, STR(val1), STR(val2), __FILE__, \
                            __LINE__);                                  \
  }

#define ASSERT_TRUE(val1) \
  { ::libc::tests::AssertTrue(val1, STR(val1), __FILE__, __LINE__); }

namespace libc {

namespace tests {

template <typename T>
void Print(const T &);

template <typename T>
void Print(T *p) {
  printf("%p", p);
}

template <typename T>
void AssertEq(const T &found, const T &expected, const char *found_str,
              const char *expected_str, const char *file, int line) {
  if (found != expected) {
    printf("Values mismatch in '%s' line %d. Found `%s` which is:\n", file,
           line, found_str);
    printf("\n");
    printf("  ");
    Print(found);
    printf("\n");
    printf("\n");
    printf("Expected `%s` which is:\n", expected_str);
    printf("\n");
    printf("  ");
    Print(expected);
    printf("\n");
    printf("\n");

    abort();
  }
}

template <typename T, typename U>
void AssertNe(const T &found, const U &expected, const char *found_str,
              const char *expected_str, const char *file, int line) {
  if (found == expected) {
    printf("Values match in '%s' line %d. Found `%s` which is:\n", file, line,
           found_str);
    printf("\n");
    printf("  ");
    Print(found);
    printf("\n");
    printf("\n");
    printf("Did not expect `%s` which is:\n", expected_str);
    printf("\n");
    printf("  ");
    Print(expected);
    printf("\n");
    printf("\n");

    abort();
  }
}

template <typename T>
void AssertGe(const T &found, const T &expected, const char *found_str,
              const char *expected_str, const char *file, int line) {
  if (found < expected) {
    printf(
        "Found value is less than expected value in '%s' line %d. Found `%s` "
        "which is:\n",
        file, line, found_str);
    printf("\n");
    printf("  ");
    Print(found);
    printf("\n");
    printf("\n");
    printf("Expected `%s` which is:\n", expected_str);
    printf("\n");
    printf("  ");
    Print(expected);
    printf("\n");
    printf("\n");

    abort();
  }
}

template <typename T>
void AssertTrue(const T &found, const char *found_str, const char *file,
                int line) {
  if (!found) {
    printf("Expected true in '%s' line %d. Found `%s` which is:\n", file, line,
           found_str);
    printf("\n");
    printf("  ");
    Print(bool(found));
    printf("\n");
    printf("\n");

    abort();
  }
}

#define RUN_TESTF(framework, test) framework.Run(test, STR(test))
#define RUN_TEST(test) RunTest(test, STR(test))

void RunTest(void (*test)(), const char *test_name);

template <typename Subclass>
class TestFramework {
 public:
  using test_func_t = void (*)(Subclass &);

  void Run(test_func_t func, const char *test_name) {
    Setup();
    printf("Running `%s` ... ", test_name);
    func(static_cast<Subclass &>(*this));
    printf("Passed\n");
    Teardown();
  }

 protected:
  virtual void Setup() {}
  virtual void Teardown() {}
};

}  // namespace tests

}  // namespace libc

#endif  // LIBC_INCLUDE_LIBC_TESTS_TEST_H_
