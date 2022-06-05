#ifndef LIBC_INCLUDE_LIBC_STARTUP_VFS_H_
#define LIBC_INCLUDE_LIBC_STARTUP_VFS_H_

#ifndef __KERNEL__

#include <libc/ustar.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace libc {
namespace startup {

class VFSNode {
 public:
  const std::string &getName() const { return name_; }

  virtual ~VFSNode() {}

  virtual bool isFile() const = 0;
  bool isDir() const { return !isFile(); }

  std::string getAbsPath() const;
  VFSNode *getParent() const { return parent_; }

 protected:
  // This shouldn't be created directly. Instead users should create Dirs or
  // Files.
  // TODO: Replace with a std::string_view.
  VFSNode(const std::string &name, VFSNode *parent);

  // TODO: This vector of bools isn't very space-efficient.
  void TreeImpl(std::vector<bool> &last) const;

 private:
  const std::string name_;
  VFSNode *parent_;
};

class File;

class Dir : public VFSNode {
 public:
  // Create an empty directory with this name.
  Dir(const std::string &name, VFSNode *parent) : VFSNode(name, parent) {}
  Dir(const std::string &cleaned_name, const DirInfo &, VFSNode *parent)
      : Dir(cleaned_name, parent) {}

  bool HasNode(const std::string &name) const { return getNode(name); }

  VFSNode *getNode(const std::string &name) const {
    for (const std::unique_ptr<VFSNode> &n : nodes_) {
      if (n->getName() == name) return n.get();
    }
    return nullptr;
  }

  Dir *getDir(const std::string &name) const {
    if (VFSNode *node = getNode(name)) {
      if (node->isDir()) return static_cast<Dir *>(node);
    }
    return nullptr;
  }

  File *getFile(const std::string &name) const;

  void Add(std::unique_ptr<VFSNode> node) {
    assert(!HasNode(node->getName()));
    nodes_.push_back(std::move(node));
  }

  void Tree() const {
    std::vector<bool> last;
    TreeImpl(last);
  }

  const auto &getNodes() const { return nodes_; }

  bool isFile() const override { return false; }

  class iterator {
   public:
    using ItTy = std::vector<std::unique_ptr<VFSNode>>::iterator;

    iterator(ItTy vec_it) : it_(vec_it) {}

    bool operator!=(const iterator &other) const { return it_ != other.it_; }
    bool operator==(const iterator &other) const { return it_ == other.it_; }

    iterator &operator++() {
      ++it_;
      return *this;
    }

    iterator operator++(int) {
      iterator cpy = *this;
      ++it_;
      return cpy;
    }

    VFSNode &operator*() { return *(*it_); }

   private:
    ItTy it_;
  };

  iterator begin() { return iterator(nodes_.begin()); }
  iterator end() { return iterator(nodes_.end()); }
  const iterator begin() const { return iterator(nodes_.begin()); }
  const iterator end() const { return iterator(nodes_.end()); }

 private:
  std::vector<std::unique_ptr<VFSNode>> nodes_;
};

// The root directory is slightly different from other dirs in that it is
// nameless and cannot be moved.
class RootDir : public Dir {
 public:
  RootDir() : Dir("/", /*parent=*/nullptr) {}
};

class File : public VFSNode {
 public:
  File(const std::string &name, const char *data_loc, size_t data_size,
       VFSNode *parent)
      : VFSNode(name, parent), data_(data_loc), size_(data_size) {}

  File(const std::string &cleaned_name, const FileInfo &info, VFSNode *parent)
      : File(cleaned_name, info.data, info.size, parent) {}

  bool isFile() const override { return true; }
  const char *getData() const { return data_; }
  size_t getSize() const { return size_; }

 private:
  const char *data_;
  size_t size_;
};

void InitVFS(RootDir &root, uintptr_t raw_vfs_data);

std::string GetAbsPath(const RootDir &root, const VFSNode &node);

bool IsAbsPath(const std::string &path);
bool IsRelPath(const std::string &path);
VFSNode *GetNodeFromAbsPath(std::string path);

Dir *GetCurrentDir();
VFSNode *GetNodeFromRelPath(std::string path, const Dir *cwd = GetCurrentDir());

inline VFSNode *GetNodeFromPath(const std::string &path) {
  if (path.empty()) return nullptr;
  if (IsAbsPath(path)) return GetNodeFromAbsPath(path);
  return GetNodeFromRelPath(path);
}

}  // namespace startup
}  // namespace libc

#endif  // __KERNEL__

#endif  // LIBC_INCLUDE_LIBC_STARTUP_VFS_H_
