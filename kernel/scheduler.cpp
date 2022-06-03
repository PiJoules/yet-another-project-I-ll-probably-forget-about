#include <assert.h>
#include <kernel/gdt.h>
#include <kernel/isr.h>
#include <kernel/kmalloc.h>
#include <kernel/scheduler.h>
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

struct TaskNode {
  TaskNode(Task *task, TaskNode *next) : task(task), next(next) {}

  Task *task;
  TaskNode *next;
};

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
    // assert(SegmentRegisterValueIsValid(static_cast<uint16_t>(regs.ss)));
    assert(static_cast<uint16_t>(regs.ss) == 0x23);
  }
}

}  // namespace

void PrintPagesMappingPhysical(uintptr_t paddr) {
  TaskNode *node = gTaskQueue;
  assert(node);
  paddr = pmm::PageAddress(paddr);
  printf("Checking vaddrs mapping to paddr 0x%x\n", paddr);
  do {
    printf("task %p\n", node->task);
    const auto *pd = node->task->getPageDir().get();
    for (size_t i = 0; i < pmm::kNumPageDirEntries; ++i) {
      if (pmm::PageAddress(pd[i]) == paddr) {
        printf("%u) 0x%x maps\n", i, pd[i]);
      }
    }
  } while ((node = node->next));
}

void Schedule(isr::registers_t *regs) {
  if (regs) { ValidateRegs(GetCurrentTask(), *regs); }

  assert(gTaskQueue);
  assert(gTaskQueue->task);

  // This is the only task in the queue (which should be the kernel).
  // In this case, just don't do anything.
  if (!gTaskQueue->next) {
    assert(gTaskQueue->task == gKernelTask);
    return;
  }

  // Cycle throught the queue.
  TaskNode *last_node = gTaskQueue->next;
  while (last_node->next) last_node = last_node->next;

  TaskNode *first_node = gTaskQueue;
  Task *current_task = gTaskQueue->task;
  Task *new_task = first_node->next->task;

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
    // Place the current task at the end of the queue.
    last_node->next = first_node;
    assert(new_task != gTaskQueue->task);
    gTaskQueue = first_node->next;
    first_node->next = nullptr;
    assert(gTaskQueue);

    // Save the registers into the current task.
    current_task->setRegs(*regs);

    KTRACE("jumping from current task %p @0x%x\n", current_task, regs->eip);
  } else {
    gTaskQueue = first_node->next;
    assert(gTaskQueue);

    KTRACE("DELETING task %p\n", current_task);

    // Delete the current task and its node.
    delete current_task;
    delete first_node;
  }

  KTRACE("SWITCH to %p @IP = 0x%x\n", new_task, new_task->getRegs().eip);

  // Load the new registers as our arguments.
  ValidateRegs(*new_task, new_task->getRegs());

  // Set the next kernel stack for the next interrupt.
  gdt::SetKernelStack(new_task->getKernelStackBase());

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

  timer::RegisterTimerCallback(0, Schedule);
}

void Destroy() {
  assert(gTaskQueue);
  assert(!gTaskQueue->next && "Expected only the kernel task to remain.");
  delete gTaskQueue;
  delete gKernelTask;
}

void RegisterTask(Task &task) {
  assert(gTaskQueue);
  TaskNode *last_node = gTaskQueue;
  while (last_node->next) last_node = last_node->next;

  last_node->next = new TaskNode(&task, /*next=*/nullptr);
}

Task &GetCurrentTask() { return *gTaskQueue->task; }
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
    if (node->task == task) return true;
  } while ((node = node->next));
  return false;
}

}  // namespace scheduler
