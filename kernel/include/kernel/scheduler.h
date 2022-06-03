#ifndef KERNEL_INCLUDE_KERNEL_SCHEDULER_H_
#define KERNEL_INCLUDE_KERNEL_SCHEDULER_H_

#ifndef ASM_FILE

#include <kernel/isr.h>
#include <kernel/paging.h>

namespace scheduler {

void Initialize();
void Destroy();

class Task {
 public:
  static constexpr size_t kDefaultKernStackSize = 0x2000;  // 2kB stack

  Task(bool user, paging::PageDirectory4M &pd, Task *parent);
  ~Task();

  bool isUser() const { return is_user_; }
  void setRegs(const isr::registers_t &regs) { regs_ = regs; }
  void setEntry(uintptr_t entry) { regs_.eip = entry; }
  void setArg(uint32_t arg) {
    // On i386, we set the first argument in EAX.
    regs_.eax = arg;
  }
  const isr::registers_t &getRegs() const { return regs_; }
  uintptr_t getKernelStackBase() const;
  paging::PageDirectory4M &getPageDir() const { return *pd_; }
  Task *getParent() const { return parent_; }

  void RecordOwnedPage(uint32_t ppage);
  void RemoveOwnedPage(uint32_t ppage);
  bool PageIsRecorded(uint32_t ppage) const;

 private:
  friend void Initialize();

  // This is used by the kernel for initializing the main kernel task.
  Task()
      : Task(/*user=*/false, paging::GetKernelPageDirectory(),
             /*parent=*/nullptr) {}

  const bool is_user_;
  isr::registers_t regs_{};

  // This is a temporary kernel stack unique to this task. It can be used for
  // things like the esp0 stack for TSS. Note that since the stack grows down,
  // this allocation actually points at the end of the stack.
  void *kernel_stack_allocation_;

  paging::PageDirectory4M *pd_;
  Task *parent_;

  // TODO: To avoid implementing vectors, instead have a set buffer size for
  // pages "owned" by this task.
  // FIXME: Might be more space-efficient to have a linked-list of pages
  // rather than a static table which will likely be mostly empty.
  static constexpr size_t kMaxPages = 256;
  uint32_t owned_phys_pages_[kMaxPages]{};
};

void RegisterTask(Task &task);
Task &GetCurrentTask();
Task &GetMainKernelTask();

bool IsRunningTask(Task *);

// If regs is non-null, save the regs passed into the current task and switch
// to the next task. Otherwise, destroy the current task and switch to the next
// one.
// FIXME: Would be better to abstract this such that its parameter is the next
// task to schedule.
void Schedule(isr::registers_t *regs);

void PrintPagesMappingPhysical(uintptr_t paddr);

}  // namespace scheduler

#endif  // ifndef ASM_FILE

// These should match the offset values in jump_args_t::regs. These offsets are
// important bc they determine how we access this struct pointer in asm.
#define GS_OFFSET 0
#define FS_OFFSET 2
#define ES_OFFSET 4
#define DS_OFFSET 6
#define EDI_OFFSET 8
#define ESI_OFFSET 12
#define EBP_OFFSET 16
#define ESP_OFFSET 20
#define EBX_OFFSET 24
#define EDX_OFFSET 28
#define ECX_OFFSET 32
#define EAX_OFFSET 36

#define EIP_OFFSET 48
#define CS_OFFSET 52
#define EFLAGS_OFFSET 56
#define USERESP_OFFSET 60
#define SS_OFFSET 64

#endif  // KERNEL_INCLUDE_KERNEL_SCHEDULER_H_
