#ifndef __KERNEL__

#include <assert.h>
#include <libc/startup/startparams.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscalls.h>

namespace libc {
namespace startup {

// Allocate a page in this process that holds all the stuff needed by libc to
// enter `main` and return the address to it. libc will be in charge of
// decoding it appropriately.
//
// The page will be in the following format:
//
//   Offset   Value
//   [0-4)    argc
//   [4-8)    argv[0] = S0
//   [8-12)   argv[1] = S1
//   [12-16)  argv[2] = S2
//            ...
//   [N-N+4)  argv[N] = SN (there will be `argc` of these entries)
//   [S0-S1)  str0
//   [S1-S2)  str1
//   [S2-S3)  str2
//            ...
//   [SN-SM)  strN
//
void PackParams(uintptr_t load_addr, const ArgvParam *params,
                size_t num_params) {
  assert(load_addr % alignof(uintptr_t) == 0);

  size_t page_size = syscall::PageSize();

  // FIXME(0): I've run into an issue when running with QEMU TCG where physcial
  // memory inexplicably gets overwritten with zeros. I suspect this memset may
  // have something to do with it, but I don't fully understand. Once we have a
  // working testing environment, we'll write as many tests as we can such that
  // we assert everything is working as intended. If we uncover a bug via test-
  // driven development, then hopefully any fixes we have will also end up
  // fixing this bug. Once we confidently reach that point, come back and
  // uncomment this.
  // memset(reinterpret_cast<void *>(load_addr), 0, page_size);

  // First thing in the page is argc.
  static_assert(sizeof(num_params) == sizeof(int));
  memcpy(reinterpret_cast<void *>(load_addr), &num_params, sizeof(int));

  // Second thing in the page is argv, a table of char pointers to elsewhere in
  // this page.
  uintptr_t argv_loc = load_addr + sizeof(int);
  assert(argv_loc % alignof(char **) == 0);
  static_assert(alignof(char **) == alignof(uintptr_t));
  char **argv_dst = reinterpret_cast<char **>(argv_loc);

  // Third is the actual strings.
  // Attempt to copy the argvs into the page.
  char *strs_dst =
      reinterpret_cast<char *>(argv_loc + sizeof(char *) * num_params);
  const char *end = reinterpret_cast<char *>(load_addr + page_size);

  for (size_t i = 0; i < num_params; ++i) {
    const ArgvParam &param = params[i];
    size_t len = param.size;
    assert(strs_dst + len + 1 < end);

    memcpy(strs_dst, param.arg, len);
    strs_dst[len] = 0;

    // This page can be loaded anywhere. Store the offsets between the current
    // location and where the string is. The unpacker will edit the argv area
    // to add the current location to the argv value. This functions similarly
    // to a relocation.
    argv_dst[i] =
        reinterpret_cast<char *>(reinterpret_cast<uintptr_t>(strs_dst) -
                                 reinterpret_cast<uintptr_t>(&argv_dst[i]));

    strs_dst += len + 1;
  }
}

void UnpackParams(uintptr_t params_addr, int &argc, char **&argv) {
  assert(params_addr % alignof(int) == 0);
  assert((params_addr + sizeof(int)) % alignof(char **) == 0);
  argc = *reinterpret_cast<int *>(params_addr);
  argv = reinterpret_cast<char **>(params_addr + sizeof(int));
  for (int i = 0; i < argc; ++i) {
    argv[i] += reinterpret_cast<uintptr_t>(&argv[i]);
  }
}

}  // namespace startup
}  // namespace libc

#endif  // __KERNEL__
