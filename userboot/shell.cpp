#include <assert.h>
#include <stdio.h>
#include <unistd.h>

namespace {

constexpr const char CR = 13;  // Carriage return

void DebugRead(char *buffer) {
  while (1) {
    int c = getchar();
    assert(c != EOF);
    if (c == CR) {
      *buffer = 0;
      putchar('\n');
      return;
    }

    *(buffer++) = static_cast<char>(c);
    putchar(c);
  }
}

}  // namespace

int main(int, char **) {
  printf("=== Interactive Shell ===\n");
  constexpr size_t kBuffSize = 1024;
  char buff[kBuffSize] = {};
  char *cwd = getcwd(buff, kBuffSize);
  if (!cwd) {
    printf("Could not find cwd\n");
    return -1;
  }

  printf("cwd: '%s'\n", cwd);
  while (1) {
    printf("$ ");
    DebugRead(buff);
    char *argv[] = {buff, NULL};
    execv(buff, argv);
  }
}
