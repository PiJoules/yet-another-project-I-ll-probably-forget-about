#ifndef LIBC_INCLUDE_DIRENT_H_
#define LIBC_INCLUDE_DIRENT_H_

#include <_internals.h>
#include <stdint.h>
#include <sys/types.h>

__BEGIN_CDECLS

#define DNAME_SIZE 256

struct dirent {
  // These are the only two required members according to:
  // https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/dirent.h.html
  ino_t d_ino;             /* Inode number */
  char d_name[DNAME_SIZE]; /* Null-terminated filename */
};

// DIR will be an opaque type.
struct DIR;

// The opendir() function shall open a directory stream corresponding to the
// directory named by the dirname argument. The directory stream is positioned
// at the first entry.
//
// Upon successful completion, opendir() shall return a pointer to an object of
// type DIR. Otherwise, a null pointer shall be returned and errno set to
// indicate the error.
DIR *opendir(const char *);

// The readdir() function shall return a pointer to a structure representing the
// directory entry at the current position in the directory stream specified by
// the argument dirp, and position the directory stream at the next entry. It
// shall return a null pointer upon reaching the end of the directory stream.
// The structure dirent defined in the <dirent.h> header describes a directory
// entry.
//
// If a file is removed from or added to the directory after the most recent
// call to opendir() or rewinddir(), whether a subsequent call to readdir()
// returns an entry for that file is unspecified.
dirent *readdir(DIR *);

void closedir(DIR *);

__END_CDECLS

#endif  // LIBC_INCLUDE_DIRENT_H_
