#ifndef LIBC_INCLUDE_LIBC_STARTUP_GLOBALSTATE_H_
#define LIBC_INCLUDE_LIBC_STARTUP_GLOBALSTATE_H_

#ifndef __KERNEL__

#include <libc/startup/vfs.h>
#include <stddef.h>
#include <string.h>
#include <syscalls.h>

namespace libc {
namespace startup {

// This is unique per-process.
struct GlobalState {
  uintptr_t argv_page;
  uintptr_t vfs_page;
  uintptr_t envp_page;
};

Dir *GetCurrentDir();
RootDir *GetGlobalFS();
GlobalState *GetGlobalState();

// This class makes it easier to edit the environment by separating the keys
// and values into their own allocations in a dynamic container.
class Envp {
 public:
  void Add(const std::string &key, const std::string &val) {
    envp_.emplace_back(key, val);
  }

  size_t size() const { return envp_.size(); }

  // NOTE: This is modifiable, so if we expose this pointer to the various `env`
  // functions, they can override this string and mess up the data structure.
  // That is indicative though of faulty application behavior since the pointers
  // returned by `getenv` should only be modifiable by the other `env` functions
  // and nothing else.
  std::string *getVal(const std::string &key) {
    for (EnvpPair &pair : envp_) {
      if (pair.key == key) return &pair.val;
    }
    return nullptr;
  }

  void setVal(const std::string &key, const std::string &val) {
    std::string *found_val = getVal(key);
    if (!found_val) {
      Add(key, val);
    } else {
      *found_val = val;
    }
  }

  // Copy `{{key}}={{val}}` into `dst`, and advance `dst` by the number of
  // bytes copied (including the null terminator).
  void CopyEntry(char *&dst, size_t i) const {
    const std::string &key = envp_[i].key;
    const std::string &val = envp_[i].val;
    memcpy(dst, key.c_str(), key.size());
    dst += key.size();
    *(dst++) = '=';
    memcpy(dst, val.c_str(), val.size());
    dst += val.size();
    *(dst++) = 0;
  }

  // This allocates a buffer that contains all (1) the actual envp table that
  // can be accessed like `environ`, and (2) the strings that this table points
  // to.
  std::unique_ptr<char[]> getPlainEnvp() const {
    // This is the size of all the pointers (including the ending NULL) in the
    // table.
    size_t total_size = (envp_.size() + 1) * sizeof(char *);

    // This accounts for all the keys, values, '='s, and null terminators.
    for (const auto &pair : envp_) {
      total_size += pair.key.size();  // key
      total_size += 1;                // '='
      total_size += pair.val.size();  // val
      total_size += 1;                // null terminator
    }

    std::unique_ptr<char[]> buff(new char[total_size]);
    ApplyRelative(buff.get());
    return buff;
  }

  // Apply the envp table and the strings they point to at `dst`. When storing
  // the pointers to strings, add an optional offset to the stored value. This
  // is useful if the address this value will be loaded from is different from
  // where it is stored.
  //
  // TODO: This operates similar to the argv packing. We should probably have a
  // generic class represeinting "relative" tables that we can pack and unpack.
  void ApplyRelative(char *dst, int32_t ptr_offset = 0) const {
    char **start = reinterpret_cast<char **>(dst);
    char **end = start + envp_.size() + 1;
    char *strings_start = reinterpret_cast<char *>(end);
    for (size_t i = 0; i < envp_.size(); ++i) {
      start[i] = strings_start + ptr_offset;
      CopyEntry(strings_start, i);
    }
    start[envp_.size()] = 0;
  }

 private:
  struct EnvpPair {
    std::string key;
    std::string val;

    EnvpPair(const std::string &key, const std::string &val)
        : key(key), val(val) {}
  };

  std::vector<EnvpPair> envp_;
};

Envp *GetEnvp();

void SetCurrentDir(Dir *wd);

// Create a page containing the `GlobalState` which is owned by another
// process. Return the virtual address in that new process this page can be
// accessed.
uintptr_t ApplyGlobalState(const GlobalState &state,
                           syscall::handle_t other_proc);

// Create a page containing data that will be unpacked from the `vfs_page`
// provided with the global state.
uintptr_t ApplyVFSData(uintptr_t data_loc, size_t size,
                       syscall::handle_t other_proc);

// Create a page containing the environment variables.
uintptr_t ApplyEnvp(const Envp &envp, syscall::handle_t other_proc);

void UnpackEnvp(char *const envp[], Envp &envp_vec);

}  // namespace startup
}  // namespace libc

#endif  // __KERNEL__

#endif  // LIBC_INCLUDE_LIBC_STARTUP_GLOBALSTATE_H_
