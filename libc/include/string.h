#ifndef STRING_H_
#define STRING_H_

#include <_internals.h>
#include <stdint.h>

__BEGIN_CDECLS

void *memcpy(void *dst, const void *src, size_t num);
void *memset(void *ptr, int value, size_t size);
void *memmove(void *dest, const void *src, size_t size);
size_t strlen(const char *str);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int memcmp(const void *lhs, const void *rhs, size_t);
char *strncpy(char *dst, const char *src, size_t num);
char *strcpy(char *dst, const char *src);
char *strstr(const char *str, const char *substr);
const char *strchr(const char *str, int ch);

__END_CDECLS

#endif
