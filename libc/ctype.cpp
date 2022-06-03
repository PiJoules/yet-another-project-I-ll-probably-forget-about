#include <ctype.h>

bool isspace(char c) {
  switch (c) {
    case ' ':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
      return true;
    default:
      return false;
  }
}

bool isprint(int c) {
  const unsigned ch = static_cast<unsigned>(c);
  return static_cast<int>((ch - ' ') < 95);
}
