#include <stdio.h>

namespace stacktrace {

namespace {

struct StackFrame {
  StackFrame *ebp;
  uintptr_t eip;
};

}  // namespace

void PrintStackTrace() {
  StackFrame *stack;
  asm volatile("mov %%ebp, %0" : "=r"(stack));
  printf("Stack trace (pipe this through llvm-symbolizer):\n");
  for (size_t frame = 0; stack; ++frame) {
    printf("0x%x\n", stack->eip);
    stack = stack->ebp;
  }
}

}  // namespace stacktrace
