#include <kernel/irq.h>
#include <kernel/isr.h>
#include <kernel/paging.h>
#include <kernel/scheduler.h>
#include <kernel/syscalls.h>
#include <kernel/timer.h>
#include <stdio.h>

using ::paging::PageDirectory4M;
using ::scheduler::Task;

namespace exceptions {
namespace {

// TODO: Would be cleaner to wrap this and the other syscalls in a class rather
// than having a global here. Or even better, all user pointers are wrapped
// with a class whose constructor takes this page directory and those are passed
// as arguments instead.
PageDirectory4M *gOldPageDir = nullptr;

void HandleKernelException(isr::registers_t *regs) {
  auto &task = scheduler::GetCurrentTask();
  printf("unhandled %s %d in task %p: %s\n",
         regs->int_no < 32 ? "exception" : "interrupt", regs->int_no, &task,
         regs->int_no < 32 ? isr::ExceptionStr(regs->int_no) : "Unknown");

  printf("- task was %s mode\n", task.isUser() ? "user" : "supervisor");

  regs->Dump();
  paging::PageDirectory4M &pd = scheduler::GetCurrentTask().getPageDir();
  pd.DumpMappedPages();
  pmm::Dump();

  // TODO: we're better than this
  abort();
}

void DispatchUserException(isr::registers_t *regs) {
  if (regs->int_no == isr::kPageFault) {
    paging::PageFaultHandler(regs);
  } else {
    // TODO: Allow for user processes to create custom exception handlers.
    printf("unhandled %s %d in task %p: %s\n",
           regs->int_no < 32 ? "exception" : "interrupt", regs->int_no,
           &scheduler::GetCurrentTask(),
           regs->int_no < 32 ? isr::ExceptionStr(regs->int_no) : "Unknown");

    regs->Dump();
    paging::PageDirectory4M &pd = scheduler::GetCurrentTask().getPageDir();
    pd.DumpMappedPages();
  }

  // This kills the current user task and removes it from the queue.
  // TODO: Would be preferable to pass something meaningful to the user.
  scheduler::Schedule(/*regs=*/nullptr, /*retval=*/1);
  abort();
}

void DispatchKernelException(isr::registers_t *regs) {
  switch (regs->int_no) {
    case isr::kPageFault:
      paging::PageFaultHandler(regs);
      abort();
      return;
    case IRQ0:
      return timer::TimerCallback(regs);
    case syscalls::kSyscallHandler:
      return syscalls::SyscallHandler(regs);
    default:
      return HandleKernelException(regs);
  }
}

void ExceptionDispatcher(isr::registers_t *regs) {
  gOldPageDir = &paging::GetCurrentPageDirectory();
  paging::SwitchPageDirectory(paging::GetKernelPageDirectory());

  switch (regs->int_no) {
    case IRQ0:
      timer::TimerCallback(regs);
      break;
    case syscalls::kSyscallHandler:
      syscalls::SyscallHandler(regs);
      break;
    default: {
      Task &current_task = scheduler::GetCurrentTask();
      if (current_task.isUser())
        DispatchUserException(regs);
      else
        DispatchKernelException(regs);
      break;
    }
  }

  paging::SwitchPageDirectory(*gOldPageDir);
}

}  // namespace

PageDirectory4M &GetPageDirBeforeException() { return *gOldPageDir; }

void InitializeHandlers() {
  for (size_t i = 0; i < isr::kNumIsrHandlers; ++i) {
    isr::RegisterIsrHandler(static_cast<uint8_t>(i), ExceptionDispatcher);
  }
}

}  // namespace exceptions
