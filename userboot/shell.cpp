#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

namespace {

constexpr size_t kBuffSize = 1024;

constexpr char TAB = 9;    // Tab
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

std::vector<std::string> GetFilesInDir(const std::string &dirname) {
  std::vector<std::string> files;
  DIR *mydir = opendir(dirname.c_str());
  if (!mydir) return files;

  struct dirent *myfile;
  while ((myfile = readdir(mydir)) != NULL) { files.push_back(myfile->d_name); }
  closedir(mydir);

  return files;
}

std::vector<std::string> GetRunnableCommands(const std::string &prefix) {
  std::vector<std::string> cmds;

  auto add_files_in_dir = [](const std::string &dirname,
                             const std::string &prefix,
                             std::vector<std::string> &cmds) {
    std::vector<std::string> files = GetFilesInDir(dirname);
    for (const std::string &file : files) {
      if (file.starts_with(prefix)) {
        // FIXME: Should also check if it's an executable.
        cmds.push_back(file);
      }
    }
  };

  if (const char *cwd_ = getenv("PATH")) {
    std::string cwd(cwd_);
    while (!cwd.empty()) {
      size_t sep = cwd.find(':');
      std::string path(cwd, 0, sep);
      add_files_in_dir(path, prefix, cmds);
      if (sep == std::string::npos) break;
      cwd.erase(sep + 1);
    }
  }

  char buff[kBuffSize]{};
  if (getcwd(buff, kBuffSize)) { add_files_in_dir(buff, prefix, cmds); }

  return cmds;
}

void LTrimWhitespace(std::string &s) {
  while (!s.empty() && isspace(s[0])) s.erase(0, 1);
}

void PrintPS1() {
  char buff[kBuffSize]{};
  if (!getcwd(buff, kBuffSize)) return;
  printf("%s $ ", buff);
}

// Return the amount to adjust the buffer.
size_t TryAutocomplete(char *const buffer, size_t size) {
  std::string prefix(buffer, size);
  LTrimWhitespace(prefix);
  if (prefix.empty()) return 0;

  std::vector<std::string> cmds = GetRunnableCommands(prefix);
  if (cmds.empty()) return 0;

  if (cmds.size() == 1) {
    // Autocomplete the buffer.
    std::string remaining(cmds[0], prefix.size());
    memcpy(buffer, cmds[0].c_str(), cmds[0].size());
    printf("%s", remaining.c_str());
    return remaining.size();
  }

  // Print suggestions.
  printf("\n");
  for (const std::string &cmd : cmds) { printf("%s  ", cmd.c_str()); }
  printf("\n");
  PrintPS1();
  printf("%s", prefix.c_str());
  return 0;
}

void DebugRead(char *buffer) {
  char *const buff_start = buffer;
  while (1) {
    int c = getchar();
    assert(c != EOF);
    if (c == CR) {
      *buffer = 0;
      putchar('\n');
      return;
    } else if (c == DEL) {
      if (buffer <= buff_start) continue;

      MoveLeft();
      putchar(' ');
      MoveLeft();

      --buffer;
      continue;
    } else if (c == TAB) {
      size_t adj =
          TryAutocomplete(buff_start, static_cast<size_t>(buffer - buff_start));
      buffer += adj;
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
  char buff[kBuffSize] = {};

  while (1) {
    PrintPS1();

    DebugRead(buff);
    const char *path;
    std::vector<char *> argv = TransformIntoArgv(buff, path);
    if (argv.size() == 1) {
      // This was just all whitepace.
      continue;
    }

    // See
    // https://www.gnu.org/software/bash/manual/html_node/Shell-Builtin-Commands.html
    // for list of builtin shell commands.
    if (strcmp(path, "cd") == 0) {
      if (argv.size() == 2) {
        // Just `cd`, which would mean go to wherever $HOME is, but since we
        // don't have a concept of a "$HOME" yet, just ignore it.
        continue;
      }

      if (argv.size() != 3) {
        printf("cd: too many arguments\n");
        continue;
      }

      if (chdir(argv[1]) != 0) {
        if (errno == ENOTDIR) {
          printf("cd: %s: Not a directory\n", argv[1]);
        } else {
          // errno == ENOENT
          printf("cd: %s: No such file or directory\n", argv[1]);
        }
      }

      continue;
    }

    int res = execv(path, argv.data());
    if (res < 0) {
      printf("%s: command not found\n", path);
    } else {
      wait(nullptr);
    }
  }
}
