#ifndef USERBOOT_INCLUDE_STARTPARAMS_H_
#define USERBOOT_INCLUDE_STARTPARAMS_H_

#ifndef __KERNEL__

#include <status.h>
#include <string.h>

namespace libc {
namespace startup {

struct ArgvParam {
  const char *arg;
  size_t size;

  ArgvParam(const char *s) : arg(s), size(strlen(arg)) {}
  ArgvParam(const char *arg, size_t size) : arg(arg), size(size) {}
};

void PackParams(uintptr_t load_addr, const ArgvParam *params,
                size_t num_params);
size_t PackSize(const ArgvParam *params, size_t num_params);
void UnpackParams(uintptr_t params_addr, int &argc, char **&argv);

}  // namespace startup
}  // namespace libc

#endif  // __KERNEL__

#endif  // USERBOOT_INCLUDE_STARTPARAMS_H_
