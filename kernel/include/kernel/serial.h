#ifndef KERNEL_INCLUDE_SERIAL_H_
#define KERNEL_INCLUDE_SERIAL_H_

#include <kernel/kernel.h>
#include <stdarg.h>

namespace serial {

void Initialize();
void Put(char);
void Write(const char *);
__PRINTF_LIKE__
void WriteF(const char *, ...);
void WriteF(const char *, va_list);
bool TryPut(char);
bool TryRead(char &);

}  // namespace serial

#endif  // KERNEL_INCLUDE_SERIAL_H_
