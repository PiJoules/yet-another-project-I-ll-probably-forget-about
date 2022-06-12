#include <status.h>
#include <stdint.h>
#include <syscalls.h>

namespace syscall {

void DebugWrite(const char *str) {
  asm volatile(
      "movl %0, %%eax\n"
      "movl %1, %%ebx\n"
      "int $0x80" ::"r"(SYS_DebugWrite),
      "r"((uint32_t)str));
}

void ProcessKill(uint32_t retval) {
  asm volatile("int $0x80" ::"a"(SYS_ProcessKill), "b"(retval));
}

kstatus_t AllocPage(uintptr_t &page_addr, handle_t proc_handle,
                    uint32_t flags) {
  kstatus_t status;
  asm volatile("int $0x80"
               : "=a"(status), "=b"(page_addr)
               : "0"(SYS_AllocPage), "1"(page_addr), "c"(proc_handle),
                 "d"(flags));
  return status;
}

size_t PageSize() {
  uint32_t page_size;
  asm volatile("int $0x80" : "=a"(page_size) : "0"(SYS_PageSize));
  return page_size;
}

kstatus_t ProcessCreate(handle_t &proc_handle) {
  kstatus_t status;
  asm volatile("int $0x80"
               : "=a"(status), "=b"(proc_handle)
               : "0"(SYS_ProcessCreate));
  return status;
}

kstatus_t MapPage(uintptr_t vaddr, handle_t other_proc, uintptr_t &other_vaddr,
                  uint32_t flags) {
  kstatus_t status;
  asm volatile("int $0x80"
               : "=a"(status), "=b"(other_vaddr)
               : "0"(SYS_MapPage), "1"(vaddr), "c"(other_proc),
                 "d"(other_vaddr), "S"(flags));
  return status;
}

void ProcessStart(handle_t proc, uintptr_t entry, uint32_t arg) {
  asm volatile("int $0x80" ::"a"(SYS_ProcessStart), "b"(proc), "c"(entry),
               "d"(arg));
}

void UnmapPage(uintptr_t page_addr) {
  asm volatile("int $0x80" ::"a"(SYS_UnmapPage), "b"(page_addr));
}

kstatus_t DebugRead(char &c) {
  kstatus_t status;
  asm volatile("int $0x80" : "=a"(status) : "0"(SYS_DebugRead), "b"(&c));
  return status;
}

kstatus_t ProcessWait(handle_t proc, uint32_t signals,
                      uint32_t &received_signal, uint32_t &signal_val) {
  kstatus_t status;
  asm volatile("int $0x80"
               : "=a"(status), "=b"(received_signal), "=c"(signal_val)
               : "0"(SYS_ProcessWait), "1"(proc), "2"(signals));
  return status;
}

kstatus_t ProcessInfo(handle_t proc, uint32_t kind, void *dst,
                      size_t buffer_size, size_t &written_or_needed) {
  kstatus_t status;
  asm volatile("int $0x80"
               : "=a"(status), "=b"(written_or_needed)
               : "0"(SYS_ProcessInfo), "1"(proc), "c"(kind), "d"(dst),
                 "S"(buffer_size));
  return status;
}

}  // namespace syscall
