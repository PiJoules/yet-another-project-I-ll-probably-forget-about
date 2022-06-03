#include <stdarg.h>
#include <stdint.h>
#include <string.h>

namespace {

using put_func_t = void (*)(char);

int Format(put_func_t, const char *, va_list);
int Format(put_func_t, const char *, ...);

template <typename IntTy, size_t kNumDigits>
void PrintDecimalImpl(put_func_t put, IntTy val) {
  constexpr uint32_t kBase = 10;
  char buffer[kNumDigits + 1];  // 10 + 1 for the null terminator
  buffer[kNumDigits] = '\0';
  char *buffer_ptr = &buffer[kNumDigits];
  do {
    --buffer_ptr;
    *buffer_ptr = (val % kBase) + '0';
    val /= kBase;
  } while (val);
  while (buffer_ptr < &buffer[kNumDigits]) put(*(buffer_ptr++));
}

void PrintDecimal(put_func_t put, uint32_t val) {
  // The largest value for this type is 4294967295 which will require 10
  // characters to print.
  return PrintDecimalImpl<uint32_t, 10>(put, val);
}

// void PrintDecimal(put_func_t put, uint64_t val) {
//   // The largest value for this type is 18446744073709551615 which will
//   require
//   // 20 characters to print.
//   return PrintDecimalImpl<uint64_t, 20>(put, val);
// }

void PrintDecimal(put_func_t put, int32_t val) {
  if (val >= 0) return PrintDecimal(put, static_cast<uint32_t>(val));
  if (val == INT32_MIN) {
    Format(put, "-2147483648");
    return;
  }

  // We can safely negate without overflow.
  put('-');
  PrintDecimal(put, static_cast<uint32_t>(-val));
}

// NOTE: The PrintXXX functions do not print any prefixes like '0x' for hex.
// Assumes 0 <= val < 16
void PrintNibble(put_func_t put, uint8_t val) {
  if (val < 10)
    put(static_cast<char>('0' + val));
  else
    put(static_cast<char>('a' + val - 10));
}

// The PrintHex functions print the 2s complement representation of signed
// integers.
void PrintHex(put_func_t put, uint8_t val) {
  PrintNibble(put, val >> 4);
  PrintNibble(put, val & 0xf);
}

void PrintHex(put_func_t put, uint16_t val) {
  PrintHex(put, static_cast<uint8_t>(val >> 8));
  PrintHex(put, static_cast<uint8_t>(val));
}

void PrintHex(put_func_t put, uint32_t val) {
  PrintHex(put, static_cast<uint16_t>(val >> 16));
  PrintHex(put, static_cast<uint16_t>(val));
}

void PrintHex(put_func_t put, uint64_t val) {
  PrintHex(put, static_cast<uint32_t>(val >> 32));
  PrintHex(put, static_cast<uint32_t>(val));
}

// void PrintHex(put_func_t put, int32_t val) {
//   PrintHex(put, static_cast<uint32_t>(val));
// }

int Format(put_func_t put, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  int num_written = Format(put, fmt, va);
  va_end(va);
  return num_written;
}

int Format(put_func_t put, const char *fmt, va_list ap) {
  int written = 0;
  char c;
  while ((c = *(fmt++))) {
    if (c == '%') {
      c = *(fmt++);
      switch (c) {
        case 'c': {
          char other_char = static_cast<char>(va_arg(ap, int));
          put(other_char);
          break;
        }
        case 'd': {
          int i = va_arg(ap, int);
          PrintDecimal(put, i);
          break;
        }
        case 'u': {
          unsigned i = va_arg(ap, unsigned);
          PrintDecimal(put, i);
          break;
        }
        case 's': {
          char *s = va_arg(ap, char *);
          for (const char *end = s + strlen(s); s < end; ++s) put(*s);
          break;
        }
        case 'p': {
          void *p = va_arg(ap, void *);
          put('0');
          put('x');
          PrintHex(put, reinterpret_cast<uintptr_t>(p));
          break;
        }
        case 'x': {
          unsigned u = va_arg(ap, unsigned);
          PrintHex(put, u);
          break;
        }
        case 'l': {
          c = *fmt;

          // TODO: Could be lots of other things.
          switch (c) {
            case 0: {
              // End of string. Printing a long.
              long l = va_arg(ap, long);
              PrintDecimal(put, static_cast<uint32_t>(l));
              return 0;
            }
            case 'l': {
              ++fmt;
              c = *fmt;

              switch (c) {
                case 'x': {  // %llx
                  unsigned long long llx = va_arg(ap, unsigned long long);
                  static_assert(sizeof(llx) == sizeof(uint64_t));
                  PrintHex(put, static_cast<uint64_t>(llx));
                  break;
                }
                case 'i':
                case 'd':
                  // TODO: Handle these.
                  __builtin_trap();
                default:
                  return -1;
              }

              ++fmt;
              break;
            }
            case 'x': {
              // Print 64 bit hex.
              unsigned long v = va_arg(ap, unsigned long);
              static_assert(sizeof(v) == sizeof(uint32_t));
              PrintHex(put, static_cast<uint32_t>(v));
              ++fmt;
              break;
            }
            default:
              // Unknown format specifier.
              return -1;
          }
          break;
        }
        default:
          return -1;
      }
    } else {
      put(c);
    }
  }

  return written;
}

}  // namespace

#ifdef __KERNEL__
#include <kernel/serial.h>

extern "C" int printf(const char *s, ...) {
  va_list va;
  va_start(va, s);
  Format(serial::Put, s, va);
  va_end(va);

  // This is a non-compliant printf.
  return 0;
}

#else
#include <syscalls.h>

extern "C" int printf(const char *s, ...) {
  va_list va;
  va_start(va, s);

  // TODO: Maybe should have something along the lines of formatting the string
  // into a buffer then do a debug write.
  auto put = [](char c) {
    const char s[] = {c, 0};
    syscall::DebugWrite(s);
  };
  Format(put, s, va);
  va_end(va);

  // This is a non-compliant printf.
  return 0;
}

extern "C" int putchar(int ch) {
  auto put = [](char c) {
    const char s[] = {c, 0};
    syscall::DebugWrite(s);
  };
  put(static_cast<char>(ch));
  return ch;
}

#endif
