#include <errno.h>
#include <libc/startup/globalstate.h>
#include <libc/startup/vfs.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <string>

char *getcwd(char *buf, size_t size) {
  if (!buf || size == 0) {
    errno = EINVAL;
    return NULL;
  }

  const libc::startup::Dir *cwd = libc::startup::GetCurrentDir();
  assert(cwd);

  std::string abspath = cwd->getAbsPath();
  if (abspath.size() > size - 1) {
    errno = ERANGE;
    return NULL;
  }

  size_t cpysize = std::min(abspath.size(), size - 1);
  memcpy(buf, abspath.c_str(), cpysize);
  buf[cpysize] = 0;
  return buf;
}
