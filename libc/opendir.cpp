#ifndef __KERNEL__

#include <dirent.h>
#include <libc/startup/globalstate.h>
#include <libc/startup/vfs.h>

using ::libc::startup::Dir;
using ::libc::startup::File;
using ::libc::startup::VFSNode;

struct DIR {
  // This struct holds a Dir iterator, which does pretty much what we want
  // the opaque DIR struct to do.
  Dir::iterator it;
  const Dir::iterator end;

  // Since `closedir` should only be called after all operations done on a DIR,
  // then this DIR will be "alive" for as long as these operations are done.
  // We can just modify this dirent for `readdir` operations.
  dirent entry;
};

DIR *opendir(const char *dirname) {
  VFSNode *dir = libc::startup::GetNodeFromPath(dirname);
  if (!dir || !dir->isDir()) return nullptr;
  DIR *dirp = new DIR{
      .it = static_cast<Dir *>(dir)->begin(),
      .end = static_cast<Dir *>(dir)->end(),
  };
  return dirp;
}

dirent *readdir(DIR *dirp) {
  if (dirp->it == dirp->end) return nullptr;

  VFSNode &node = *(dirp->it);
  const std::string &name = node.getName();
  size_t len = std::min(sizeof(dirent::d_name) - 1, name.size());
  memcpy(dirp->entry.d_name, name.c_str(), len);
  dirp->entry.d_name[len] = 0;
  dirp->it++;

  // TODO: What should go in `d_ino`?
  return &dirp->entry;
}

void closedir(DIR *dirp) {
  if (dirp) { delete dirp; }
}

#endif  // __KERNEL__
