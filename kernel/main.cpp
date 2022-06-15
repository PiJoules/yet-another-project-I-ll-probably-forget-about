#include <kernel/exceptions.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/irq.h>
#include <kernel/isr.h>
#include <kernel/kernel.h>
#include <kernel/kmalloc.h>
#include <kernel/multiboot.h>
#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <kernel/scheduler.h>
#include <kernel/serial.h>
#include <kernel/syscalls.h>
#include <kernel/tests.h>
#include <kernel/timer.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>

/* Check if the bit BIT in FLAGS is set. */
#define CHECK_FLAG(flags, bit) ((flags) & (1 << (bit)))

extern "C" {

extern uint32_t __KERNEL_BEGIN;
extern uint32_t __KERNEL_END;
extern void GDTFlush(uint32_t);
extern void TSSFlush();

}  // extern "C"

namespace {

void InitPmm(uintptr_t mem_upper, uintptr_t kernel_begin,
             multiboot::multiboot_memory_map_t *mmaps, size_t num_mmaps) {
  // Initialize physical memory management.
  pmm::Initialize(mem_upper);
  assert(pmm::GetNumFree4MPages() == pmm::GetNum4MPages() &&
         "Expected all pages to be initially marked as free.");

  // Mark the page the kernel is on as used.
  pmm::SetPageUsed(pmm::AddrToPage(kernel_begin));

  // Then mark any available pages.
  for (size_t i = 0; i < num_mmaps; ++i) {
    auto &mmap = mmaps[i];
    if (mmap.type == MULTIBOOT_MEMORY_AVAILABLE) {
      assert(mmap.addr <= UINTPTR_MAX);
      assert(mmap.len <= UINTPTR_MAX);
      uintptr_t start = static_cast<uintptr_t>(mmap.addr);
      assert(start + static_cast<size_t>(mmap.len) >= start);
      uintptr_t end =
          RoundUp(start + static_cast<size_t>(mmap.len), pmm::kPageSize4M);
      uint32_t start_page = pmm::AddrToPage(start);
      uint32_t end_page = pmm::AddrToPage(end);
      assert(end_page > start_page);
      do {
        if (pmm::PageIsUsed(start_page)) pmm::SetPageFree(start_page);
      } while (++start_page < end_page);
    }
  }

  // Finally mark pages as reserved. This should be last since a page range
  // marked as available could immedately be followed by an unabailable start
  // address.
  for (size_t i = 0; i < num_mmaps; ++i) {
    auto &mmap = mmaps[i];
    if (mmap.type != MULTIBOOT_MEMORY_AVAILABLE && mmap.len > 0) {
      assert(mmap.addr <= UINTPTR_MAX);
      assert(mmap.len <= UINTPTR_MAX);
      uint64_t start = mmap.addr;
      assert(start + mmap.len >= start);
      uint64_t end = RoundUp(start + mmap.len, pmm::kPageSize4M);
      uint32_t start_page = static_cast<uint32_t>(pmm::AddrToPage(start));
      uint32_t end_page = static_cast<uint32_t>(pmm::AddrToPage(end));
      assert(end_page > start_page);
      do {
        if (!pmm::PageIsUsed(start_page)) pmm::SetPageUsed(start_page);
      } while (++start_page < end_page);
    }
  }

  // Finally just mark the page the kernel is on as available.
  assert(!pmm::PageIsUsed(pmm::AddrToPage(kernel_begin)) &&
         "Expected the kernel to be on an available page.");
  pmm::SetPageUsed(pmm::AddrToPage(kernel_begin));
}

void JumpToUserMode(uintptr_t initrd_start, uintptr_t initrd_end) {
  assert(!InterruptsAreEnabled());
  assert(initrd_start < initrd_end);
  printf("Initrd start: 0x%x\n", initrd_start);
  printf("Initrd end (inclusive): 0x%x\n", initrd_end);

  // The initial ramdisk will be the user program we jump into. We will setup
  // one user page for it to be placed on, but after that it's up to the user
  // program to do anything else.
  //
  // Setup the initial user page directory. We will only allocate one page for
  // the user. If they want more, they need to request more via syscalls. Just
  // to be nice, we'll also ensure the first user process doesn't get mapped to
  // address zero.
  //
  // Note that the kernel will not guarantee which page the first user program
  // will be loaded in. So userboot *should* be PC-relative code.
  paging::PageDirectory4M *user_pd = paging::GetKernelPageDirectory().Clone();
  int32_t free_vpage =
      paging::GetCurrentPageDirectory().getNextFreePage(/*lower_bound=*/1);
  assert(free_vpage > 0);
  uintptr_t user_start = pmm::PageToAddr(static_cast<uint32_t>(free_vpage));
  int32_t free_ppage = pmm::GetNextFreePage();
  assert(free_ppage >= 0);
  user_pd->MapPage(user_start,
                   pmm::PageToAddr(static_cast<uint32_t>(free_ppage)),
                   /*flags=*/PG_USER);
  user_pd->Memcpy(user_start, initrd_start,
                  std::min(initrd_end - initrd_start, pmm::kPageSize4M));

  printf("userboot entry: %p\n", (void *)user_start);
  auto *init_user_task = new scheduler::Task(/*user=*/true, *user_pd,
                                             &scheduler::GetMainKernelTask());
  printf("initial user task: %p\n", init_user_task);
  init_user_task->setEntry(user_start);
  RegisterTask(*init_user_task);

  // The timer will start after this call. This will start the scheduler which
  // will switch between different tasks.
  EnableInterrupts();

  while (1) {}
}

// Setup the actual kernel here using copied values from the multiboot.
// Do this just so we don't accidentally reference multiboot structs after
// setting up paging.
void KernelSetup(uintptr_t mem_upper, multiboot::multiboot_memory_map_t *mmaps,
                 size_t num_mmaps, uintptr_t initrd_start,
                 uintptr_t initrd_end) {
  uintptr_t kernel_begin = reinterpret_cast<uintptr_t>(&__KERNEL_BEGIN);
  uintptr_t kernel_end = reinterpret_cast<uintptr_t>(&__KERNEL_END);
  assert(kernel_begin % pmm::kPageSize4M == 0 &&
         "Expected the kernel to start on a page boundary.");

  size_t kernel_size = kernel_end - kernel_begin;
  assert(kernel_size <= pmm::kPageSize4M &&
         "Expected the kernel to fit in a page");
  printf("Kernel is %u KB large (physical range: %p - %p)\n", kernel_size >> 10,
         &__KERNEL_BEGIN, &__KERNEL_END);

  InitPmm(mem_upper, kernel_begin, mmaps, num_mmaps);
  printf("Num available pages: %u\n", pmm::GetNumFree4MPages());
  pmm::Dump();

  paging::Initialize();
  kmalloc::Initialize();
  const size_t avail_kheap = kmalloc::GetAvailMemory();

  gdt::Initialize();
  idt::Initialize();
  isr::Initialize();
  irq::Initialize();
  timer::Initialize();
  scheduler::Initialize();
  exceptions::InitializeHandlers();

  const size_t init_used_pages = pmm::GetNumUsed4MPages();

  tests::RunKernelTests();

  if (initrd_start && initrd_end) {
    JumpToUserMode(initrd_start, initrd_end);
  } else {
    printf(
        "No initial ramdisk found. Could not jump into any userspace "
        "program.\n");
  }

  scheduler::Destroy();

  // END OF KERNEL = Do some cleanup.
  // We expect only the page for the kernel to be mapped.
  if (init_used_pages != pmm::GetNumUsed4MPages()) {
    printf("Found %u in use! Expected only %u to be left in use.\n",
           pmm::GetNumUsed4MPages(), init_used_pages);
    LOOP_INDEFINITELY();
  }

  if (kmalloc::GetAvailMemory() != avail_kheap) {
    printf(
        "Found %u of heap available. Expected the initial heap size of %u.\n",
        kmalloc::GetAvailMemory(), avail_kheap);
    LOOP_INDEFINITELY();
  }
}

}  // namespace

// NOTE: We must be very careful when using this multiboot because it could
// be anywhere in memory. We should make sure that once we initialize paging,
// that we don't need to reference the original struct in case we overwrite
// it. Any important information needed from it should be copied elsewhere.
extern "C" void kernel_main(const multiboot::Multiboot *boot, uint32_t magic) {
  serial::Initialize();

  serial::Write("\n");

  if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
    printf("Unknown header magic 0x%x\n", magic);
    abort();
  }

  boot->Dump();

  // Starting from here, we will stop directly referencing the multiboot struct.
  // Anything needed from it should be copied elsewhere.
  uintptr_t mem_upper = boot->getMemUpper();

  // I normally see around 8 mmaps when testing locally with QEMU, but set to
  // 16 for a bit more space.
  constexpr size_t kMmapsBuffer = 16;
  multiboot::multiboot_memory_map_t mmaps[kMmapsBuffer] = {};
  size_t num_mmaps = 0;
  boot->getMmap(mmaps, sizeof(mmaps), num_mmaps);
  assert(num_mmaps > 0);
  assert(num_mmaps <= kMmapsBuffer);

  uintptr_t mod_start = 0, mod_end = 0;
  if (boot->hasModules() && boot->mods_count) {
    if (boot->mods_count > 1) {
      printf(
          "Found more than one module. Using the first one for the initial "
          "ramdisk.");
    }

    auto *mod =
        reinterpret_cast<multiboot::Multiboot::ModuleInfo *>(boot->mods_addr);
    mod_start = mod->mod_start;
    mod_end = mod->mod_end;
  }

  boot = nullptr;
  KernelSetup(mem_upper, mmaps, num_mmaps, mod_start, mod_end);

  serial::Write("End of kernel!\n");
}
