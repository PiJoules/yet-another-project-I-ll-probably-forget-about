#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {
  constexpr size_t kBuffSize = 1024;
  char buff[kBuffSize] = {};

  const char *dir;
  if (argc >= 2) {
    dir = argv[1];
  } else {
    dir = getcwd(buff, kBuffSize);
    assert(dir);
  }

  DIR *mydir = opendir(dir);
  if (!mydir) {
    printf("No directory named '%s'\n", dir);
    return 1;
  }
  struct dirent *myfile;
  while ((myfile = readdir(mydir)) != NULL) { printf("%s\n", myfile->d_name); }
  closedir(mydir);
  return 0;
}
