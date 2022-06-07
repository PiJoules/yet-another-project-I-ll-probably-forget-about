#ifndef KERNEL_INCLUDE_KERNEL_STATUS_H_
#define KERNEL_INCLUDE_KERNEL_STATUS_H_

enum kstatus_t {
  K_OK = 0,

  // Out of physical memory.
  K_OOM_PHYS = 1,

  // There are no more virtual pages available.
  K_OOM_VIRT = 2,

  // The virtual address we are attempting to map is already mapped.
  K_VPAGE_MAPPED = 3,

  // The buffer passed to a syscall is too small.
  K_BUFFER_TOO_SMALL = 4,

  // A handle argument is invalid.
  K_INVALID_HANDLE = 5,

  // An address we expected to be page-aligned is not page-aligned.
  K_UNALIGNED_PAGE_ADDR = 6,

  // Some read was unsuccessful.
  K_UNABLE_TO_READ = 7,

  // Some argument was invalid.
  K_INVALID_ARG = 8,
};

#endif  // KERNEL_INCLUDE_KERNEL_STATUS_H_
