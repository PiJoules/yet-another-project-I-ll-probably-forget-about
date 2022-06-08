#include <errno.h>
#include <libc/startup/globalstate.h>
#include <libc/startup/vfs.h>
#include <unistd.h>

int chdir(const char *path) {
  libc::startup::VFSNode *node = libc::startup::GetNodeFromPath(path);
  if (!node) {
    errno = ENOENT;
    return -1;
  }

  if (!node->isDir()) {
    errno = ENOTDIR;
    return -1;
  }

  auto *dir = static_cast<libc::startup::Dir *>(node);
  libc::startup::SetCurrentDir(dir);

  return 0;
}
