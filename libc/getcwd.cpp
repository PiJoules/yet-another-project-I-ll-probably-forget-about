#if !defined(__KERNEL__) && !defined(__USERBOOT_STAGE1__)

#include <libc/startup/globalstate.h>
#include <libc/startup/vfs.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <string>

char *getcwd(char *buf, size_t size) {
  const libc::startup::Dir *cwd = libc::startup::GetCurrentDir();
  if (!cwd) return nullptr;

  std::string abspath = cwd->getAbsPath();
  memcpy(buf, abspath.c_str(), std::min(size, abspath.size()));
  return buf;
}

#endif  // !defined(__KERNEL__) && !defined(__USERBOOT_STAGE1__)
