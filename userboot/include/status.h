#ifndef USERBOOT_INCLUDE_STATUS_H_
#define USERBOOT_INCLUDE_STATUS_H_

#define K_OK 0

// Out of physical memory.
#define K_OOM_PHYS 1

// There are no more virtual pages available.
#define K_OOM_VIRT 2

// The virtual address we are attempting to map is already mapped.
#define K_VPAGE_MAPPED 3

// The buffer passed to a syscall is too small.
#define K_BUFFER_TOO_SMALL 4

// A handle argument is invalid.
#define K_INVALID_HANDLE 5

// An address we expected to be page-aligned is not page-aligned.
#define K_UNALIGNED_PAGE_ADDR 6

// Some read was unsuccessful.
#define K_UNABLE_TO_READ 7

// Some argument was invalid.
#define K_INVALID_ARG 8

#ifndef ASM_FILE
#include <stdint.h>
typedef uint32_t kstatus_t;
#endif  // ASM_FILE

#endif  // USERBOOT_INCLUDE_STATUS_H_
