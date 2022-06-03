#include <assert.h>
#include <stdio.h>

int main(int argc, char **argv) {
  assert(argc == 2);
  assert(argv);

  printf("Hello %s!\n", argv[1]);
}
