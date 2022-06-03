#ifndef KERNEL_INCLUDE_KERNEL_PANIC_H_
#define KERNEL_INCLUDE_KERNEL_PANIC_H_

[[noreturn]] void __panic(const char *msg, const char *file, int line);

#define PANIC(msg) __panic(msg, __FILE__, __LINE__)

#endif  // KERNEL_INCLUDE_KERNEL_PANIC_H_
