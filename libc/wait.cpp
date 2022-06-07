#ifndef __KERNEL__

#define LOCAL_DEBUG_LVL 0

#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>
#include <syscalls.h>

#include <memory>

using syscall::handle_t;

// TODO: While the kernel doesn't support process return codes, what we can do
// is abstract this out such that we have some kind of IPC where the running
// child process returns some sort of return code that this parent accepts here.
pid_t wait(int *) {
  // Get the current task handle.
  handle_t this_handle;
  size_t written_or_needed;
  DEBUG_OK(syscall::ProcessInfo(/*proc=*/0, PROC_CURRENT, &this_handle,
                                sizeof(this_handle), written_or_needed));

  DEBUG_PRINT("current handle: 0x%x\n", this_handle);

  // Get the children. By calling this syscall with a size of zero, we get the
  // size of the buffer we need.
  // NOTE: It's possible that by the time we reach this call that any child
  // tasks may have completed already. This is ok for the purposes of `wait`,
  // but once we want to get the return code, then we will need some way of
  // doing the IPC (or implementing return codes) and wait to get some status
  // before the child task completes.
  kstatus_t status =
      syscall::ProcessInfo(this_handle, PROC_CHILDREN, /*dst=*/nullptr,
                           /*buffer_size=*/0, written_or_needed);
  if (status == K_OK) {
    // There are no children to wait for.
    DEBUG_ASSERT(written_or_needed == 0);
    return 0;
  }

  DEBUG_ASSERT(status == K_BUFFER_TOO_SMALL);
  DEBUG_ASSERT(written_or_needed >= sizeof(handle_t) &&
               "Expected at least one handle.");
  DEBUG_ASSERT(written_or_needed % sizeof(handle_t) == 0);
  DEBUG_PRINT("needed buff size: %u bytes (%u handles)\n", written_or_needed,
              written_or_needed / sizeof(handle_t));

  // Actually get the child threads.
  // FIXME: The number of tasks can actually change if more get added or some
  // complete in between calling this. Rather than just guessing and checking,
  // it might be better to implement a suspension mechanism for pausing tasks
  // then getting info from them.
  size_t num_handles = written_or_needed / sizeof(handle_t);
  std::unique_ptr<handle_t[]> children(new handle_t[num_handles]);
  status = syscall::ProcessInfo(this_handle, PROC_CHILDREN, children.get(),
                                written_or_needed, written_or_needed);
  DEBUG_OK(status);

  // Finally wait on the child. `wait` only needs to wait for one of the
  // children, but it doesn't matter which one we wait on, so just pick the
  // first one. NOTE: A return status of `K_INVALID_HANDLE` means the child is
  // no longer running.
  status = syscall::ProcessWait(children[0], TASK_TERMINATED);
  DEBUG_ASSERT(status == K_OK || status == K_INVALID_HANDLE);

  return 0;
}

#endif  // __KERNEL__
