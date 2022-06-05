#include <errno.h>

static int __errno = 0;

int *__errno_location() { return &__errno; }
