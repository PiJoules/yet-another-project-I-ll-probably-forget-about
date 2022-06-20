#ifndef KERNEL_INCLUDE_KERNEL_EXCEPTIONS_H_
#define KERNEL_INCLUDE_KERNEL_EXCEPTIONS_H_

#include <kernel/paging.h>

namespace exceptions {

void InitializeHandlers();
paging::PageDirectory4M &GetPageDirBeforeException();

}  // namespace exceptions

#endif  // KERNEL_INCLUDE_KERNEL_EXCEPTIONS_H_
