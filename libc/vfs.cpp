#define LOCAL_DEBUG_LVL 0

#include <ctype.h>
#include <libc/startup/globalstate.h>
#include <libc/startup/startparams.h>
#include <libc/startup/vfs.h>
#include <libc/ustar.h>
#include <stdio.h>

#include <memory>
#include <string>
#include <vector>

using libc::DirInfo;
using libc::FileInfo;

namespace libc {
namespace startup {

namespace {

template <typename InfoTy>
std::string FullPath(const InfoTy &info) {
  std::string path(info.prefix);
  path += info.name;
  return path;
}

constexpr char kPathSep = '/';

// Returns true if a character should be stripped.
using should_strip_func_t = bool (*)(char c);

void InplaceLStrip(std::string &path,
                   should_strip_func_t should_strip = isspace) {
  if (path.empty()) return;
  while (should_strip(path[0])) path.erase(path.begin());
}

void InplaceRStrip(std::string &path,
                   should_strip_func_t should_strip = isspace) {
  if (path.empty()) return;
  while (1) {
    auto it = path.end() - 1;
    if (!should_strip(*it)) return;
    path.erase(it);
  }
}

void CleanPath(std::string &path) {
  InplaceLStrip(path);
  if (path.size() >= 2) {
    if (path[0] == '.' && path[1] == kPathSep) path.erase(0, 2);
  }

  InplaceRStrip(path);

  // Be careful not to strip the root dir path ("/").
  if (path.size() == 1 && path[0] == kPathSep) return;
  InplaceRStrip(path, [](char c) { return c == kPathSep; });
}

template <typename NodeTy, typename InfoTy>
class NodeBuilder {
 public:
  NodeBuilder(const InfoTy &info) : info_(info) {}

  void ParsePathRelative(std::string &path, Dir &cwd) {
    assert(IsRelPath(path));
    assert(*path.end() != kPathSep);
    assert(!isspace(path[0]));
    assert(!isspace(*path.end()));
    assert(!path.empty());

    // Paths at this point:
    // - Have no trailing or leading whitespace
    // - Are relative to the current working directory
    // - Are not empty

    size_t path_sep_loc = path.find(kPathSep);
    if (path_sep_loc == std::string::npos) {
      // This is the base node.
      cwd.Add(new NodeTy(path, info_, &cwd));
      return;
    }

    // The current value up to the path separator is a dir.
    auto dirname = path.substr(0, path_sep_loc);
    path.erase(0, path_sep_loc + 1);

    if (Dir *existing_dir = cwd.getDir(dirname)) {
      // This directory exists.
      if (!path.empty()) {
        // Move down the path and add nodes to this directory.
        ParsePathRelative(path, *existing_dir);
      }
    } else {
      // Create this directory.
      std::unique_ptr<VFSNode> dir(new Dir(dirname, &cwd));
      if (!path.empty()) {
        // Move down the path and add nodes to this directory.
        ParsePathRelative(path, static_cast<Dir &>(*dir));
      }
      cwd.Add(std::move(dir));
    }
  }

 private:
  const InfoTy &info_;
};

class DirBuilder : public NodeBuilder<Dir, DirInfo> {
 public:
  DirBuilder(const DirInfo &info) : NodeBuilder(info) {}
};

class FileBuilder : public NodeBuilder<File, FileInfo> {
 public:
  FileBuilder(const FileInfo &info) : NodeBuilder(info) {}
};

// NOTE: This will edit `path`, so any strings that need to be maintained
// should pass a copy.
VFSNode *GetNodeFromRelPathImpl(std::string &path, const Dir &search_path) {
  assert(!path.empty());
  assert(path[0] != kPathSep);
  assert(*path.end() != kPathSep);

  size_t path_sep_loc = path.find(kPathSep);
  if (path_sep_loc == std::string::npos) {
    // No more directories. Check if the CWD has this node.
    return search_path.getNode(path);
  }

  // The current value up to the path separator is a dir.
  auto dirname = path.substr(0, path_sep_loc);
  path.erase(0, path_sep_loc + 1);

  if (Dir *existing_dir = search_path.getDir(dirname)) {
    // This directory exists. Do a recursive check.
    return GetNodeFromRelPathImpl(path, *existing_dir);
  }

  // This path doesn't exist.
  return nullptr;
}

VFSNode *GetNodeFromPathImpl(const std::string &path, bool search_path_env);

Dir *GetDirFromPathEnv(const std::string &path) {
  VFSNode *found_path = GetNodeFromPathImpl(path, /*search_path_env=*/false);
  if (!found_path || !found_path->isDir()) return nullptr;
  return static_cast<Dir *>(found_path);
}

class PathEnvIterator {
 public:
  static constexpr char kPathEnvSep = ':';

  PathEnvIterator(const std::string &path) : path_(path), search_start_(0) {}

  bool ReachedEnd() const { return search_start_ >= path_.size(); }

  operator bool() const { return !ReachedEnd(); }

  PathEnvIterator &operator++() {
    if (!ReachedEnd()) search_start_ = path_.find(kPathEnvSep, search_start_);
    return *this;
  }

  // TODO: Might be better to use a string_view here.
  std::string operator*() const {
    size_t next_path_idx = path_.find(kPathEnvSep, search_start_);
    return path_.substr(search_start_, next_path_idx);
  }

 private:
  const std::string &path_;
  size_t search_start_;
};

// TODO: Account for parent directories.
// `search_path_env` prevents infinite recursion. From a higher level call, we
// also want to check search directories for relative paths.
//
// `path` is pass by value because it's passed to `GetNodeFromRelPathImpl`
// which should receive references to edit.
VFSNode *GetNodeFromRelPath(std::string path, const Dir *cwd,
                            bool search_path_env) {
  if (!cwd) return nullptr;
  CleanPath(path);

  std::string *path_env;
  if (search_path_env && (path_env = GetEnvp()->getVal("PATH"))) {
    // Found the PATH.
    PathEnvIterator iter(*path_env);
    while (iter) {
      std::string path_dir(*iter);
      if (Dir *dir = GetDirFromPathEnv(path_dir)) {
        // This path exists.
        std::string path_cpy(path);
        if (VFSNode *node = GetNodeFromRelPathImpl(path_cpy, *dir)) {
          return node;
        }
      }
      ++iter;
    }
  }

  return GetNodeFromRelPathImpl(path, *cwd);
}

VFSNode *GetNodeFromAbsPath(std::string path) {
  const Dir *root = GetGlobalFS();
  assert(root);

  CleanPath(path);
  if (!IsAbsPath(path)) return nullptr;
  if (path.size() == 1 && path[0] == kPathSep)
    return libc::startup::GetGlobalFS();  // Root dir

  assert(path[0] == kPathSep);
  path.erase(0, 1);

  return GetNodeFromRelPath(path, root, /*search_path_env=*/false);
}

VFSNode *GetNodeFromPathImpl(const std::string &path, bool search_path_env) {
  if (path.empty()) return nullptr;
  if (IsAbsPath(path)) return GetNodeFromAbsPath(path);
  return GetNodeFromRelPath(path, GetCurrentDir(), search_path_env);
}

}  // namespace

VFSNode *GetNodeFromPath(const std::string &path) {
  return GetNodeFromPathImpl(path, /*search_path_env=*/true);
}

bool IsAbsPath(const std::string &path) { return path[0] == kPathSep; }
bool IsRelPath(const std::string &path) { return !IsAbsPath(path); }

VFSNode::VFSNode(const std::string &name, VFSNode *parent)
    : name_(name), parent_(parent) {
  if (parent_) { assert(parent_->isDir()); }
}

void VFSNode::TreeImpl(std::vector<bool> &last) const {
  if (!last.empty()) {
    for (size_t i = 0; i < last.size() - 1; ++i) {
      if (last[i])
        printf("   ");
      else
        printf("|  ");
    }

    if (last.back())
      printf("`--");
    else
      printf("|--");
  }

  printf("%s\n", getName().c_str());

  if (isFile()) return;

  const Dir *dir = static_cast<const Dir *>(this);
  if (dir->getNodes().empty()) return;

  for (size_t i = 0; i < dir->getNodes().size() - 1; ++i) {
    last.push_back(false);
    dir->getNodes()[i]->TreeImpl(last);
    last.pop_back();
  }

  last.push_back(true);
  dir->getNodes().back()->TreeImpl(last);
  last.pop_back();
}

File *Dir::getFile(const std::string &name) const {
  if (VFSNode *node = getNode(name)) {
    if (node->isFile()) return static_cast<File *>(node);
  }
  return nullptr;
}

void InitVFS(RootDir &root, uintptr_t raw_vfs_data) {
  auto dir_callback = [](const DirInfo &info, void *arg) {
    auto *root = reinterpret_cast<RootDir *>(arg);
    std::string path(FullPath(info));
    CleanPath(path);
    if (path.empty())  // This was likely "./".
      return;

    DirBuilder(info).ParsePathRelative(path, *root);
  };
  auto file_callback = [](const FileInfo &info, void *arg) {
    auto *root = reinterpret_cast<RootDir *>(arg);
    std::string path(FullPath(info));
    CleanPath(path);
    if (path.empty())  // This was likely "./".
      return;

    FileBuilder(info).ParsePathRelative(path, *root);
  };

  libc::IterateUSTAR(raw_vfs_data, dir_callback, file_callback, /*arg=*/&root);
}

std::string VFSNode::getAbsPath() const {
  const VFSNode *parent = getParent();
  if (!parent) {
    // This is the root directory.
    return getName();
  }

  if (parent == GetGlobalFS()) return "/" + getName();

  return parent->getAbsPath() + "/" + getName();
}

}  // namespace startup
}  // namespace libc
