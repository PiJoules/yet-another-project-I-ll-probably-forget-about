#include <assert.h>
#include <kernel/gdt.h>
#include <kernel/isr.h>
#include <kernel/kmalloc.h>
#include <kernel/linkedlist.h>
#include <kernel/scheduler.h>
#include <kernel/status.h>
#include <kernel/timer.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>

// Set this to 1 to enable kernel traces.
#define LOCAL_KTRACE 0

#if LOCAL_KTRACE
#define KTRACE(...) printf("KTRACE> " __VA_ARGS__)
#else
#define KTRACE(...)
#endif

namespace scheduler {
namespace {
struct jump_args_t;
}  // namespace
}  // namespace scheduler

extern "C" void switch_task(scheduler::jump_args_t *);

namespace scheduler {
namespace {

using TaskNode = kern::LinkedList<Task *>;
TaskNode *gTaskQueue = nullptr;
Task *gKernelTask = nullptr;

struct jump_args_t {
  isr::registers_t regs;
};

static_assert(offsetof(jump_args_t, regs.gs) == GS_OFFSET);
static_assert(offsetof(jump_args_t, regs.fs) == FS_OFFSET);
static_assert(offsetof(jump_args_t, regs.es) == ES_OFFSET);
static_assert(offsetof(jump_args_t, regs.ds) == DS_OFFSET);
static_assert(offsetof(jump_args_t, regs.edi) == EDI_OFFSET);
static_assert(offsetof(jump_args_t, regs.esi) == ESI_OFFSET);
static_assert(offsetof(jump_args_t, regs.ebp) == EBP_OFFSET);
static_assert(offsetof(jump_args_t, regs.esp) == ESP_OFFSET);
static_assert(offsetof(jump_args_t, regs.ebx) == EBX_OFFSET);
static_assert(offsetof(jump_args_t, regs.edx) == EDX_OFFSET);
static_assert(offsetof(jump_args_t, regs.ecx) == ECX_OFFSET);
static_assert(offsetof(jump_args_t, regs.eax) == EAX_OFFSET);

static_assert(offsetof(jump_args_t, regs.eip) == EIP_OFFSET);
static_assert(offsetof(jump_args_t, regs.cs) == CS_OFFSET);
static_assert(offsetof(jump_args_t, regs.eflags) == EFLAGS_OFFSET);
static_assert(offsetof(jump_args_t, regs.useresp) == USERESP_OFFSET);
static_assert(offsetof(jump_args_t, regs.ss) == SS_OFFSET);

bool SegmentRegisterValueIsValid(uint32_t reg) {
  if (reg > (gdt::kUserDataSeg | gdt::kRing3)) return false;

  constexpr uint32_t kMask = 0xFC;  // All but the first 3 bits.
  return (reg & kMask) == 0x08 || (reg & kMask) == 0x10 ||
         (reg & kMask) == 0x18 || (reg & kMask) == 0x20;
}

void ValidateRegs(Task &task, const isr::registers_t &regs) {
  assert(SegmentRegisterValueIsValid(regs.cs));
  assert(SegmentRegisterValueIsValid(regs.ds));
  assert(SegmentRegisterValueIsValid(regs.gs));
  assert(SegmentRegisterValueIsValid(regs.fs));
  assert(SegmentRegisterValueIsValid(regs.es));
  assert(SegmentRegisterValueIsValid(regs.ds));

  if (task.isUser()) {
    // If this is a user task, then esp0 and ss should've been saved.

    // I can't find anything that confirms if the SS popped on an interrupt
    // is 16 or 32 bits. However, it does look like if we iret to a different
    // ring, the SS is a 32-bit pop and the high-order 16 bits are ignored.
    // So when we validate, it's ok to just check the bottom 16 bits.
    assert(static_cast<uint16_t>(regs.ss) == 0x23);
  }
}

using iter_tasks_callback_t = void (*)(Task &t, void *arg);

void IterateTasks(iter_tasks_callback_t callback, void *arg) {
  assert(gTaskQueue);
  TaskNode *node = gTaskQueue;
  do { callback(*node->get(), arg); } while ((node = node->next()));
}

}  // namespace

void Task::SendSignal(signal_t signals, uint32_t value) {
  assert(signals);
  for (const Task *listener : listening_for_signals_) {
    Signals *signal = listener->GetSignal(*this);
    if (signal && (signal->expecting_signals & signals)) {
      signal->received_signal = signals;
      signal->value = value;
    }
  }
}

void Task::WaitOn(Task &other_task, signal_t signals) {
  assert(signals);
  auto &waiting_on = GetOrCreateSignal(other_task);
  signal_t &waiting_on_signals = waiting_on.expecting_signals;
  waiting_on_signals = static_cast<signal_t>(waiting_on_signals | signals);

  other_task.AddListener(*this);
}

void Task::RemoveSignal(const Task *task) {
  auto it = waiting_on_signals_.begin();
  const auto end = waiting_on_signals_.end();
  while (it != end) {
    if ((*it).task == task) {
      waiting_on_signals_.erase(it);
      return;
    }
    ++it;
  }
  printf("COULDN'T FIND TASK %p THAT TASK %p WAS WAITING ON\n", task, this);
  __builtin_trap();
}

void PrintPagesMappingPhysical(uintptr_t paddr) {
  TaskNode *node = gTaskQueue;
  assert(node);
  paddr = pmm::PageAddress(paddr);
  printf("Checking vaddrs mapping to paddr 0x%x\n", paddr);
  do {
    printf("task %p\n", node->get());
    const auto *pd = node->get()->getPageDir().get();
    for (size_t i = 0; i < pmm::kNumPageDirEntries; ++i) {
      if (pmm::PageAddress(pd[i]) == paddr) {
        printf("%u) 0x%x maps\n", i, pd[i]);
      }
    }
  } while ((node = node->next()));
}

void Schedule(isr::registers_t *regs, uint32_t retval) {
  if (regs) { ValidateRegs(GetCurrentTask(), *regs); }

  assert(gTaskQueue);
  assert(gTaskQueue->get());

  // This is the only task in the queue (which should be the kernel).
  // In this case, just don't do anything.
  if (!gTaskQueue->next()) {
    assert(gTaskQueue->get() == gKernelTask);
    return;
  }

  TaskNode *current_node = gTaskQueue;
  Task *current_task = current_node->get();
  TaskNode *next_node = gTaskQueue->next();

  // Keep cycling until we find a task not waiting on signals.
  while (next_node && !next_node->get()->canRunTask()) {
    next_node = next_node->next();
  }

  // In this specific situation, we are switching from the main kernel task to
  // some other task. However, all the other tasks we cycled through are waiting
  // on signals from other tasks (deadlock). Technically we still have the main
  // kernal task which can run, but it will just loop forever.
  //
  // TODO: See what the appropriate thing for the kernel to do in this
  // situation.
  if (!next_node && current_task == &GetMainKernelTask()) {
    printf(
        "WARN: All userspace tasks are waiting on signals and have "
        "deadlocked!\n");
    return;
  }

  assert(next_node &&
         "If the current task is not the main kernel task, we should've found "
         "at least one node that had an active task with no signals.");

  Task *new_task = next_node->get();

  if (!current_task->isUser() && regs) {
    // If we interrupt in the middle of a kernel task, that means we at still
    // on the same stack as that task. This is because there's no privilege
    // change, so we didn't need to change the stack to esp0 in the TSS. This
    // just does some validation checks.
    assert(regs->esp % sizeof(void *) == 0);
    uint32_t *esp = reinterpret_cast<uint32_t *>(regs->esp);
    assert(esp[0] == regs->int_no);
    assert(esp[1] == regs->err_code);
    assert(esp[2] == regs->eip);
    assert(esp[3] == regs->cs);
    assert(esp[4] == regs->eflags);
  }

  if (regs) {
    // Update the queue such that it's now starting with the selected new
    // thread.
    while (gTaskQueue != next_node) gTaskQueue = gTaskQueue->Cycle();
    assert(gTaskQueue);

    // Save the registers into the current task.
    current_task->setRegs(*regs);

    KTRACE("jumping from current task %p @0x%x\n", current_task, regs->eip);
  } else {
    // First remove the current node from the queue, then update so the next
    // node is at the front.
    gTaskQueue = current_node->next();
    while (gTaskQueue != next_node) gTaskQueue = gTaskQueue->Cycle();
    assert(gTaskQueue);

    KTRACE("DELETING task %p\n", current_task);
    current_task->SendSignal(Task::kTerminated, retval);

    // Delete the current task and its node.
    delete current_task;
    delete current_node;
  }

  KTRACE("SWITCH to %p @IP = 0x%x\n", new_task, new_task->getRegs().eip);

  if (new_task->isWaitingOnSignal()) {
    uint32_t signal_val;
    const Task *sending_task;
    if (Task::signal_t received_signal =
            new_task->getReceivedSignal(&signal_val, &sending_task)) {
      // This task came from the ProcessWait syscall.
      if (new_task->isUser()) { assert(new_task->getRegs().eax == K_OK); }

      new_task->getSignalReceivedReg() = received_signal;
      new_task->getSignalValReg() = signal_val;

      // Now that we've received a signal from a task we were waiting on, remove
      // it.
      new_task->RemoveSignal(sending_task);
    }
  }

  // Load the new registers as our arguments.
  ValidateRegs(*new_task, new_task->getRegs());

  // Set the next kernel stack for the next interrupt.
  gdt::SetKernelStack(new_task->getKernelStackBase());

  // Send the running signal.
  // TODO: We only really need to send this on the very first run of this task.
  new_task->SendSignal(Task::kRunning, /*value=*/0);

  // Switch the page directory.
  paging::SwitchPageDirectory(new_task->getPageDir());

  jump_args_t args{new_task->getRegs()};
  args.regs.eflags |= 0x200;  // Re-enable interrupts.
  switch_task(&args);
}

uintptr_t Task::getKernelStackBase() const {
  assert(kernel_stack_allocation_);
  uintptr_t stack_bottom =
      reinterpret_cast<uintptr_t>(kernel_stack_allocation_) +
      kDefaultKernStackSize;
  assert(stack_bottom % sizeof(void *) == 0 &&
         "The kernel stack is not 4 byte aligned.");
  return stack_bottom;
}

Task::Task(bool user, paging::PageDirectory4M &pd, Task *parent)
    : is_user_(user),
      kernel_stack_allocation_(kmalloc::kmalloc(kDefaultKernStackSize)),
      pd_(&pd),
      parent_(parent) {
  if (user) {
    regs_.ss = regs_.ds = regs_.gs = regs_.fs = regs_.es =
        (gdt::kUserDataSeg | gdt::kRing3);
    regs_.cs = (gdt::kUserCodeSeg | gdt::kRing3);
  } else {
    regs_.ss = regs_.ds = regs_.gs = regs_.fs = regs_.es = gdt::kKernDataSeg;
    regs_.cs = gdt::kKernCodeSeg;
  }
}

Task::~Task() {
  kmalloc::kfree(kernel_stack_allocation_);

  for (uint32_t &ppage : owned_phys_pages_) {
    if (ppage) {
      assert(pmm::PageIsUsed(ppage));
      pmm::SetPageFree(ppage);
    }
  }

  if (pd_ != &paging::GetKernelPageDirectory()) delete pd_;
}

void Initialize() {
  gKernelTask = new Task();
  gdt::SetKernelStack(gKernelTask->getKernelStackBase());
  gTaskQueue = new TaskNode(gKernelTask, /*next=*/nullptr);

  timer::RegisterTimerCallback(
      0, [](isr::registers_t *regs) { return Schedule(regs, /*retval=*/0); });
}

void Destroy() {
  assert(gTaskQueue);
  assert(!gTaskQueue->next() && "Expected only the kernel task to remain.");
  delete gTaskQueue;
  delete gKernelTask;

  // TODO: Destroy `gSignals`
}

void RegisterTask(Task &task) {
  assert(gTaskQueue);
  gTaskQueue->Append(&task);
  task.SendSignal(Task::kReady, /*retval=*/0);
}

Task &GetCurrentTask() { return *gTaskQueue->get(); }
Task &GetMainKernelTask() { return *gKernelTask; }

bool Task::PageIsRecorded(uint32_t ppage) const {
  uint32_t const *page = owned_phys_pages_;
  const uint32_t *end = owned_phys_pages_ + kMaxPages;
  while (page != end) {
    if (*(page++) == ppage) return true;
  }
  return false;
}

void Task::RecordOwnedPage(uint32_t ppage) {
  assert(!PageIsRecorded(ppage) && "Physical address already recorded.");
  for (uint32_t &page : owned_phys_pages_) {
    if (!page) {
      page = ppage;
      pmm::SetPageUsed(ppage);
      return;
    }
  }

  printf("FIXME: Unable to record any more pages for this task!\n");
  abort();
}

void Task::RemoveOwnedPage(uint32_t ppage) {
  assert(PageIsRecorded(ppage) && "Physical address not recorded.");
  for (uint32_t &page : owned_phys_pages_) {
    if (page == ppage) {
      page = 0;
      pmm::SetPageFree(ppage);
      return;
    }
  }

  abort();
}

bool IsRunningTask(Task *task) {
  assert(gTaskQueue);
  TaskNode *node = gTaskQueue;
  do {
    if (node->get() == task) return true;
  } while ((node = node->next()));
  return false;
}

std::vector<Task *> Task::getChildren() const {
  struct Args {
    const Task *parent;
    std::vector<Task *> children;
  } args = {
      .parent = this,
  };
  IterateTasks(
      [](Task &task, void *arg) {
        auto *args = reinterpret_cast<Args *>(arg);
        if (task.getParent() == args->parent) {
          args->children.push_back(&task);
        }
      },
      &args);
  return args.children;
}

}  // namespace scheduler
