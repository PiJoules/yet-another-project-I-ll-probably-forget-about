#include <kernel/irq.h>
#include <kernel/isr.h>
#include <kernel/scheduler.h>
#include <kernel/syscalls.h>
#include <kernel/timer.h>
#include <stdio.h>

using ::scheduler::Task;

namespace exceptions {
namespace {

void HandleKernelException(isr::registers_t *regs) {
  printf("unhandled %s %d in task %p: %s\n",
         regs->int_no < 32 ? "exception" : "interrupt", regs->int_no,
         &scheduler::GetCurrentTask(),
         regs->int_no < 32 ? isr::ExceptionStr(regs->int_no) : "Unknown");

  regs->Dump();
  paging::PageDirectory4M &pd = scheduler::GetCurrentTask().getPageDir();
  pd.DumpMappedPages();
  pmm::Dump();

  // TODO: we're better than this
  abort();
}

void DispatchUserException(isr::registers_t *regs) {
  if (regs->int_no == isr::kPageFault) {
    return paging::PageFaultHandler(regs);
  }

  // TODO: Allow for user processes to create custom exception handlers.
  printf("unhandled %s %d in task %p: %s\n",
         regs->int_no < 32 ? "exception" : "interrupt", regs->int_no,
         &scheduler::GetCurrentTask(),
         regs->int_no < 32 ? isr::ExceptionStr(regs->int_no) : "Unknown");

  regs->Dump();
  paging::PageDirectory4M &pd = scheduler::GetCurrentTask().getPageDir();
  pd.DumpMappedPages();

  // This kills the current user task and removes it from the queue.
  // TODO: Would be preferable to pass something meaningful to the user.
  scheduler::Schedule(/*regs=*/nullptr, /*retval=*/1);
  abort();
}

void DispatchKernelException(isr::registers_t *regs) {
  switch (regs->int_no) {
    case isr::kPageFault:
      return paging::PageFaultHandler(regs);
    case IRQ0:
      return timer::TimerCallback(regs);
    case syscalls::kSyscallHandler:
      return syscalls::SyscallHandler(regs);
    default:
      return HandleKernelException(regs);
  }
}

void ExceptionDispatcher(isr::registers_t *regs) {
  switch (regs->int_no) {
    case IRQ0:
      return timer::TimerCallback(regs);
    case syscalls::kSyscallHandler:
      return syscalls::SyscallHandler(regs);
    default: {
      Task &current_task = scheduler::GetCurrentTask();
      if (current_task.isUser()) return DispatchUserException(regs);
      return DispatchKernelException(regs);
    }
  }
}

}  // namespace

void InitializeHandlers() {
  for (size_t i = 0; i < isr::kNumIsrHandlers; ++i) {
    isr::RegisterIsrHandler(static_cast<uint8_t>(i), ExceptionDispatcher);
  }
}

}  // namespace exceptions
