#include <assert.h>
#include <libc/startup/globalstate.h>
#include <stdio.h>
#include <string.h>
#include <syscalls.h>

extern "C" char **environ;

namespace libc {
namespace startup {

void UnpackEnvp(char *const envp[], Envp &envp_vec) {
  while (*envp) {
    // Environment variables are in the format `KEY=VAL`.
    std::string entry(*envp);
    auto found_sep = entry.find('=');
    if (found_sep == std::string::npos || found_sep == 0) {
      printf("WARN: envp entry '%s' not in the format 'KEY=VAL'\n",
             entry.c_str());
      continue;
    }

    envp_vec.setVal(entry.substr(0, found_sep), entry.substr(found_sep + 1));

    ++envp;
  }
}

#if !defined(__USERBOOT_STAGE1__)
// Userboot stage 1 does not have an environment to update.
void UpdateEnviron() {
  std::unique_ptr<char[]> plain_env = GetEnvp()->getPlainEnvp();
  GetPlainEnv()->swap(plain_env);
  environ = reinterpret_cast<char **>(GetPlainEnv()->get());
}
#endif  // !defined(__USERBOOT_STAGE1__)

}  // namespace startup
}  // namespace libc
