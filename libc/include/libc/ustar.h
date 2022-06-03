#ifndef LIBC_INCLUDE_LIBC_USTAR_H_
#define LIBC_INCLUDE_LIBC_USTAR_H_

#include <stdint.h>

namespace libc {

struct DirInfo {
  const char *prefix;
  const char *name;
};

struct FileInfo {
  const char *prefix;
  const char *name;
  size_t size;
  const char *data;
};

union TarBlock {
  union {
    // Pre-POSIX.1-1988 format
    struct {
      char name[100];  // file name
      char mode[8];    // permissions
      char uid[8];     // user id (octal)
      char gid[8];     // group id (octal)
      char size[12];   // size (octal)
      char mtime[12];  // modification time (octal)
      char check[8];  // sum of unsigned characters in block, with spaces in the
                      // check field while calculation is done (octal)
      char link;      // link indicator
      char link_name[100];  // name of linked file
    };

    // UStar format (POSIX IEEE P1003.1)
    struct {
      char old[156];             // first 156 octets of Pre-POSIX.1-1988 format
      char type;                 // file type
      char also_link_name[100];  // name of linked file
      char ustar[8];             // ustar\000
      char owner[32];            // user name (string)
      char group[32];            // group name (string)
      char major[8];             // device major number
      char minor[8];             // device minor number
      char prefix[155];
    };
  };

  char block[512];  // raw memory (500 octets of actual data, padded to 1 block)
};
constexpr size_t kTarBlockSize = 512;
static_assert(sizeof(TarBlock) == kTarBlockSize);
static_assert(alignof(TarBlock) == 1);

inline bool IsZeroPage(const uint8_t *tar_page) {
  for (size_t i = 0; i < kTarBlockSize; ++i) {
    if (*(tar_page++)) return false;
  }
  return true;
}

using dir_callback_t = void (*)(const DirInfo &, void *arg);
using file_callback_t = void (*)(const FileInfo &, void *arg);

// Returns the number of bytes in this archive.
size_t IterateUSTAR(uintptr_t archive, dir_callback_t dircallback,
                    file_callback_t filecallback, void *arg);

inline size_t GetTarsize(uintptr_t archive) {
  return IterateUSTAR(archive, /*dircallback=*/nullptr,
                      /*filecallback=*/nullptr, /*args=*/nullptr);
}

}  // namespace libc

#endif  // LIBC_INCLUDE_LIBC_USTAR_H_
