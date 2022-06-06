#if !defined(__KERNEL__) && !defined(__USERBOOT_STAGE1__)

#include <errno.h>
#include <libc/elf/elf.h>
#include <libc/startup/globalstate.h>
#include <libc/startup/startparams.h>
#include <libc/startup/vfs.h>
#include <unistd.h>

using libc::elf::ElfModule;
using libc::startup::Dir;

namespace {

int ExecImpl(const char *path, char *const argv[],
             const libc::startup::Envp &envp) {
  libc::startup::VFSNode *node = libc::startup::GetNodeFromPath(path);
  if (!node || !node->isFile()) {
    errno = EACCES;
    return -1;
  }

  const auto &f = static_cast<libc::startup::File &>(*node);

  // TODO: Could also just accept the vector of ArgvParam as an arg.
  std::vector<libc::startup::ArgvParam> params;
  while (char *arg = *(argv++)) { params.emplace_back(arg); }

  uintptr_t vfs_data = libc::startup::GetGlobalState()->vfs_page;
  size_t vfs_data_size = libc::GetTarsize(vfs_data);

  uintptr_t elf_data = reinterpret_cast<uintptr_t>(f.getData());
  if (elf_data % ElfModule::kMinAlign != 0) {
    std::unique_ptr<char[]> aligned_elf_data(
        new (std::align_val_t(ElfModule::kMinAlign)) char[f.getSize()]);
    assert(aligned_elf_data);
    memcpy(aligned_elf_data.get(), f.getData(), f.getSize());
    libc::elf::LoadElfProgram(
        reinterpret_cast<uintptr_t>(aligned_elf_data.get()), params.data(),
        params.size(), vfs_data, vfs_data_size, envp);
  } else {
    libc::elf::LoadElfProgram(elf_data, params.data(), params.size(), vfs_data,
                              vfs_data_size, envp);
  }

  return 0;
}

}  // namespace

int execv(const char *path, char *const argv[]) {
  return ExecImpl(path, argv, *libc::startup::GetEnvp());
}

int execve(const char *path, char *const argv[], char *const envp[]) {
  libc::startup::Envp _envp;
  libc::startup::UnpackEnvp(envp, _envp);
  return ExecImpl(path, argv, _envp);
}

#endif  // !defined(__KERNEL__) && !defined(__USERBOOT_STAGE1__)
