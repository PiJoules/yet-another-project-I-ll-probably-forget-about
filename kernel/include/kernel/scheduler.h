#ifndef KERNEL_INCLUDE_KERNEL_SCHEDULER_H_
#define KERNEL_INCLUDE_KERNEL_SCHEDULER_H_

#ifndef ASM_FILE

#include <kernel/isr.h>
#include <kernel/paging.h>

#include <vector>

namespace scheduler {

void Initialize();
void Destroy();

class Task {
 public:
  enum signal_t : uint32_t {
    // The task is ready to run, but has not yet started.
    // A process that was created, then started via a ProcessStart syscall, but
    // has not run any of it's code yet is in this state. This task is on the
    // queue to run, but has not yet run.
    kReady = 0x1,

    // The task is actively running.
    // It has been started and is running code.
    kRunning = 0x2,

    // This task is being killed. This can happen via a ProcessKill syscall.
    // Note that the task may still be alive even if this is signaled.
    kTerminated = 0x4,
  };

  static constexpr size_t kDefaultKernStackSize = 0x2000;  // 2kB stack

  Task(bool user, paging::PageDirectory4M &pd, Task *parent);
  ~Task();

  bool isUser() const { return is_user_; }
  void setRegs(const isr::registers_t &regs) { regs_ = regs; }
  void setEntry(uintptr_t entry) { regs_.eip = entry; }
  void setArg(uint32_t arg) {
#if defined(__i386__)
    // On i386, we set the first argument in EAX.
    regs_.eax = arg;
#else
#error "What arch?"
#endif
  }
  uint32_t &getSignalReceivedReg() {
#if defined(__i386__)
    return regs_.ebx;
#else
#error "What arch?"
#endif
  }
  uint32_t &getSignalValReg() {
#if defined(__i386__)
    return regs_.ecx;
#else
#error "What arch?"
#endif
  }
  const isr::registers_t &getRegs() const { return regs_; }
  uintptr_t getKernelStackBase() const;
  paging::PageDirectory4M &getPageDir() const { return *pd_; }
  Task *getParent() const { return parent_; }
  std::vector<Task *> getChildren() const;

  void RecordOwnedPage(uint32_t ppage);
  void RemoveOwnedPage(uint32_t ppage);
  bool PageIsRecorded(uint32_t ppage) const;

  // Wait for a signal from another task.
  void WaitOn(Task &other_task, signal_t signals);

  // Return true if any of the signals this task is waiting on have not been
  // handled.
  bool isWaitingOnSignal() const { return !waiting_on_signals_.empty(); }

  // If this task was waiting on a signal but has received a signal from one
  // of the tasks it was listening to, return that signal (which will be
  // non-zero) and value sent with it. If multiple signals were received, this
  // will return any one of them (no order is guaranteed).
  //
  // If this task was not waiting on a signal, return 0.
  signal_t getReceivedSignal(uint32_t *value = nullptr,
                             const Task **task = nullptr) const {
    for (Signals &signal : waiting_on_signals_) {
      if (signal.received_signal) {
        if (value) *value = signal.value;
        if (task) *task = signal.task;
        return signal.received_signal;
      }
    }
    return signal_t(0);
  }

  // This task can *not* run if:
  // - It is waiting on signals, none of which has been received yet.
  bool canRunTask() const {
    if (isWaitingOnSignal()) {
      if (!getReceivedSignal(/*value=*/nullptr)) { return false; }
    }
    return true;
  }

  void SendSignal(signal_t signal, uint32_t value);

  void RemoveSignal(const Task *task);

 private:
  friend void Initialize();

  struct Signals {
    // The other task this task is expecting a signal from.
    const Task *task;

    // The type of the signal.
    signal_t expecting_signals;

    // The signal value sent.
    uint32_t value;

    // The signal this task recevied.
    signal_t received_signal;

    Signals(const Task *task, signal_t signals, uint32_t value)
        : task(task),
          expecting_signals(signals),
          value(value),
          received_signal(signal_t(0)) {}
  };

  // This is used by the kernel for initializing the main kernel task.
  Task()
      : Task(/*user=*/false, paging::GetKernelPageDirectory(),
             /*parent=*/nullptr) {}

  Signals *GetSignal(const Task &other) const {
    for (auto &signal : waiting_on_signals_) {
      if (signal.task == &other) return &signal;
    }
    return nullptr;
  }

  Signals &GetOrCreateSignal(const Task &other) {
    Signals *expected = GetSignal(other);
    if (expected) return *expected;
    waiting_on_signals_.emplace_back(&other, /*signals=*/signal_t(0),
                                     /*value=*/0u);
    return waiting_on_signals_.back();
  }

  void AddListener(const Task &waiting_task) {
    for (const Task *t : listening_for_signals_) {
      if (t == &waiting_task) return;
    }
    listening_for_signals_.push_back(&waiting_task);
  }

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

  // A list of tasks that we expect to receive signals from.
  std::vector<Signals> waiting_on_signals_;

  // A list of tasks this task is expected to send signals to.
  std::vector<const Task *> listening_for_signals_;
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
void Schedule(isr::registers_t *regs, uint32_t retval);

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
