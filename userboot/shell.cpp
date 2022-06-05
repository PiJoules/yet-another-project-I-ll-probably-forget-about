#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

#include <string>
#include <vector>

namespace {

constexpr char CR = 13;    // Carriage return
constexpr char ESC = 27;   // Escape
constexpr char CSI = 91;   // Constrol sequence introducer
constexpr char DEL = 127;  // DEL (read on backspace key)

void MoveLeft() {
  // ANSI for moving cursor left.
  putchar(ESC);
  putchar(CSI);
  putchar('D');
}

void DebugRead(char *buffer) {
  const char *const buff_start = buffer;
  while (1) {
    int c = getchar();
    assert(c != EOF);
    if (c == CR) {
      *buffer = 0;
      putchar('\n');
      return;
    }

    if (c == DEL) {
      if (buffer <= buff_start) continue;

      MoveLeft();
      putchar(' ');
      MoveLeft();

      --buffer;
      continue;
    }

    *(buffer++) = static_cast<char>(c);
    putchar(c);
  }
}

// This buffer is a single string that was typed on the cmdline. `argv` entries
// must be null-terminated. We can just edit the original buffer replacing
// whitespace with NULLs, so our actual `argv` array can point into this buffer.
//
// Save the exe path that into `path_arg` so it can be directly passed to
// `execv`.
std::vector<char *> TransformIntoArgv(char *buff, const char *&path_arg) {
  std::vector<char *> argv;

  // In case there's any whitespace at the start, save the pointer to the
  // path.
  while (*buff && isspace(*buff)) { ++buff; }
  path_arg = buff;

  while (*buff) {
    if (isspace(*buff)) {
      *(buff++) = 0;
      continue;
    }

    argv.push_back(buff);

    // Skip through remaining printable characters until the next whitespace or
    // null-terminator.
    while (*buff && !isspace(*buff)) { ++buff; }
  }

  // The last entry in `argv` must be NULL.
  argv.push_back(NULL);
  return argv;
}

}  // namespace

int main(int, char **) {
  printf("=== Interactive Shell ===\n");
  constexpr size_t kBuffSize = 1024;
  char buff[kBuffSize] = {};
  if (!getcwd(buff, kBuffSize)) {
    printf("Could not find cwd\n");
    return -1;
  }

  const std::string cwd(buff);
  while (1) {
    printf("%s $ ", cwd.c_str());
    DebugRead(buff);
    const char *path;
    std::vector<char *> argv = TransformIntoArgv(buff, path);
    if (argv.size() == 1) {
      // This was just all whitepace.
      continue;
    }

    int res = execv(path, argv.data());
    if (res < 0) { printf("%s: command not found\n", path); }
  }
}
