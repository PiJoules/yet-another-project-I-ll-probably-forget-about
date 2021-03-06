#ifndef KERNEL_INCLUDE_IO_H_
#define KERNEL_INCLUDE_IO_H_

#include <stdint.h>

namespace io {

// Functions for reading from and writing to ports.
inline void Write8(uint16_t port, uint8_t value) {
  asm volatile("outb %1, %0" : : "dN"(port), "a"(value));
}

inline void Write16(uint16_t port, uint16_t value) {
  asm volatile("outw %1, %0" : : "dN"(port), "a"(value));
}

inline uint8_t Read8(uint16_t port) {
  uint8_t ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "dN"(port));
  return ret;
}

inline uint16_t Read16(uint16_t port) {
  uint16_t ret;
  asm volatile("inw %1, %0" : "=a"(ret) : "dN"(port));
  return ret;
}

}  // namespace io

#endif  // KERNEL_INCLUDE_IO_H_
