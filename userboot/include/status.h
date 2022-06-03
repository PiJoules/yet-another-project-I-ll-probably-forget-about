#ifndef USERBOOT_INCLUDE_STATUS_H_
#define USERBOOT_INCLUDE_STATUS_H_

#define K_OK 0

#ifndef ASM_FILE
#include <stdint.h>
typedef uint32_t kstatus_t;
#endif  // ASM_FILE

#endif  // USERBOOT_INCLUDE_STATUS_H_
