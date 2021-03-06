#ifndef STDIO_H_
#define STDIO_H_

#include <_internals.h>
#include <stdint.h>

#define EOF (-1)

__BEGIN_CDECLS

__attribute__((format(printf, 1, 2))) __EXPORT int printf(const char *s, ...);
int putchar(int c);
int puts(const char *);
int getchar();

typedef uintptr_t FILE;
FILE *popen();
int pclose(FILE *);

__END_CDECLS

#endif
