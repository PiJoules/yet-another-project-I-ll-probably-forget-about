#include <assert.h>
#include <libc/ustar.h>
#include <stdint.h>
#include <string.h>

#ifdef NDEBUG
#define DEBUG_ASSERT(x) (void)(x)
#else
#define DEBUG_ASSERT(x) assert(x)
#endif

namespace libc {

namespace {

size_t oct2bin(const char *str, size_t size) {
  size_t n = 0;
  const char *c = str;
  while (size-- > 0) {
    n *= 8;
    assert(*c >= '0');
    n += static_cast<size_t>(*c - '0');
    c++;
  }
  return n;
}

constexpr char kDirectory = '5';

}  // namespace

size_t IterateUSTAR(uintptr_t archive, dir_callback_t dircallback,
                    file_callback_t filecallback, void *arg) {
  DEBUG_ASSERT(archive % alignof(TarBlock) == 0);
  const TarBlock *tar = reinterpret_cast<const TarBlock *>(archive);
  while (!(IsZeroPage(reinterpret_cast<const uint8_t *>(tar)) &&
           IsZeroPage(reinterpret_cast<const uint8_t *>(tar + 1)))) {
    DEBUG_ASSERT(memcmp(tar->ustar, "ustar", 5) == 0 &&
                 "Expected UStar format");
    const char *prefix = tar->prefix;
    const char *name = tar->name;

    size_t filesize = oct2bin(tar->size, sizeof(TarBlock::size) - 1);
    if (tar->type == kDirectory) {
      DEBUG_ASSERT(!filesize && "A directory should have no file size.");
      DirInfo dirinfo = {
          .prefix = prefix,
          .name = name,
      };
      if (dircallback) dircallback(dirinfo, arg);
      ++tar;
      continue;
    }

    DEBUG_ASSERT(filesize);

    // Skip past the header.
    ++tar;

    // Round up to the nearest number of 512 byte chunks.
    size_t num_chunks = (filesize % kTarBlockSize)
                            ? (filesize / kTarBlockSize) + 1
                            : (filesize / kTarBlockSize);

    FileInfo fileinfo = {
        .prefix = prefix,
        .name = name,
        .size = filesize,
        .data = reinterpret_cast<const char *>(tar),
    };
    if (filecallback) filecallback(fileinfo, arg);

    tar += num_chunks;
  }

  // Be sure to add 2 more pages to indicate the ending zero-pages.
  return reinterpret_cast<uintptr_t>(tar) - archive + (kTarBlockSize * 2);
}

}  // namespace libc
