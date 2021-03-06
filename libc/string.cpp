#include <stdlib.h>
#include <string.h>

void *memset(void *ptr, int value, size_t size) {
  auto *p = static_cast<unsigned char *>(ptr);
  while (size--) {
    *p = static_cast<unsigned char>(value);
    ++p;
  }
  return ptr;
}

// TODO: We should do something similar to what glibc does and take advantage of
// larger reads/stores by starting them at properly aligned addresses. All other
// addresses should continue with a byte-by-byte read.
void *memcpy(void *dst, const void *src, size_t num) {
  const auto *p_src = static_cast<const unsigned char *>(src);
  auto *p_dst = static_cast<unsigned char *>(dst);
  while (num--) {
    *p_dst = *p_src;
    ++p_dst;
    ++p_src;
  }
  return dst;
}

// Copies the first num characters of source to destination. If the end of the
// source C string (which is signaled by a null-character) is found before num
// characters have been copied, destination is padded with zeros until a total
// of num characters have been written to it.
//
// No null-character is implicitly appended at the end of destination if source
// is longer than num. Thus, in this case, destination shall not be considered a
// null terminated C string (reading it as such would overflow).
//
// destination and source shall not overlap (see memmove for a safer alternative
// when overlapping).
char *strncpy(char *dst, const char *src, size_t num) {
  size_t len = strlen(src);
  size_t cpy = (len < num) ? len : num;
  for (size_t i = 0; i < cpy; ++i) {
    *dst = *src;
    ++dst;
    ++src;
  }
  for (size_t i = cpy; i < num; ++i) { *(dst++) = 0; }
  return dst;
}

char *strcpy(char *dst, const char *src) {
  return strncpy(dst, src, strlen(src));
}

void *memmove(void *dest, const void *src, size_t size) {
  if (!size) return dest;
  char *temp = (char *)malloc(size);
  memcpy(temp, src, size);
  memcpy(dest, temp, size);
  free(temp);
  return dest;
}

size_t strlen(const char *str) {
  size_t len = 0;
  while (str[len]) len++;
  return len;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  if (n == 0) return 0;
  while (*s1 && (*s1 == *s2) && n--) {
    ++s1;
    ++s2;
  }
  return n ? (*s1 - *s2) : 0;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    ++s1;
    ++s2;
  }
  return *s1 - *s2;
}

int memcmp(const void *lhs, const void *rhs, size_t size) {
  const auto *v1 = reinterpret_cast<const unsigned char *>(lhs);
  const auto *v2 = reinterpret_cast<const unsigned char *>(rhs);
  while (size && (*v1 == *v2)) {
    ++v1;
    ++v2;
    --size;
  }
  return *v1 - *v2;
}

// Finds the first occurrence of the null-terminated byte string pointed to by
// substr in the null-terminated byte string pointed to by str. The terminating
// null characters are not compared.
//
// Pointer to the first character of the found substring in str, or a null
// pointer if such substring is not found.
char *strstr(const char *str, const char *substr) {
  // If substr points to an empty string, str is returned.
  if (!*substr) return const_cast<char *>(str);

  for (size_t i = 0; str[i]; ++i) {
    size_t j;
    for (j = 0; str[i + j] && str[i + j] == substr[j]; ++j) {}
    if (!substr[j]) return const_cast<char *>(str + i);
  }
  return nullptr;
}

const char *strchr(const char *src, int c) {
  const char ch = static_cast<char>(c);
  for (; *src && *src != ch; ++src) {}
  return *src == ch ? const_cast<char *>(src) : nullptr;
}
