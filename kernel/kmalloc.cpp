#include <kernel/kernel.h>
#include <kernel/kmalloc.h>
#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <stdio.h>

#include <algorithm>

extern "C" uint32_t __KERNEL_BEGIN;

namespace kmalloc {

// namespace {
//
//// `MallocHeader` is a shadow allocation placed right before the pointer we
//// return from `kmalloc`.
// constexpr size_t kMinSize = 4;
// constexpr size_t kSizeBitsMax = 31;
// constexpr size_t kSizeMax = UINT32_MAX;
//// Manually set alignment here because it's possible for alignof(MallocHeader)
//// to be 1 (without alignas) since it's packed.
// struct alignas(kMinSize) MallocHeader {
//   // This refers to the size of the allocated memory following this header.
//   // Note that since the malloc headers need to be aligned, the sizes must
//   also
//   // be a multiple of the malloc header size.
//   void setSize(size_t size) {
//     assert(SizeFits(size));
//     assert(size % kMinSize == 0);
//     size_ = size;
//   }
//
//   void IncSize(size_t inc) {
//     assert(SizeFits(size_ + inc));
//     size_ += inc;
//   }
//
//   size_t getSize() const { return size_; }
//
//   void setUsed(bool used) { used_ = used; }
//
//   bool isUsed() const { return used_; }
//
//   static bool SizeFits(size_t s) { return s <= kSizeMax; }
//
//  private:
//   uint32_t size_ : kSizeBitsMax;
//   uint32_t used_ : 1;
// } __attribute__((packed));
// static_assert(sizeof(MallocHeader) == kMinSize);
// static_assert(alignof(MallocHeader) == kMinSize);
//
// constexpr size_t kMinAlign = kMinSize;
//
//// All malloc headers are aligned to 4 bytes. This also implies the size must
//// be a multiple of 4 bytes. Note this also allows zero-length allocations to
//// pass, but those should be cleaned up whenever we exit a function here.
// bool IsValid(const MallocHeader *header) {
//   return (reinterpret_cast<uintptr_t>(header) % kMinAlign == 0 &&
//           header->getSize() % kMinAlign == 0);
// }
//
//// For simplicity, we will place all kernel allocations on a single 4MB page.
//// This means we will not be able to allocate anything greater than 4MB and
//// if we hit the limit, we're done.
// constexpr size_t kMemLimit = pmm::kPageSize4M;
//
// uintptr_t gKmallocStart;
//
//// Return false to stop iterating.
// using malloc_header_callback_t = bool (*)(MallocHeader *header, void *arg);
//
// void IterMallocHeaders(malloc_header_callback_t callback, void *arg =
// nullptr) {
//   uintptr_t start = gKmallocStart;
//   uintptr_t end = start + kMemLimit;
//   while (start < end) {
//     if (!callback(reinterpret_cast<MallocHeader *>(start), arg)) return;
//     // Note that because we dereference the ptr again here, we can edit the
//     // header in place and jump safely to the next header.
//     start += sizeof(MallocHeader) +
//              reinterpret_cast<MallocHeader *>(start)->getSize();
//   }
// }
//
//// We can merge two adjascent malloc headers if they meet any of the following
//// criteria:
//// - The current one is free and the next one is size zero.
//// - The current one and the next one are free.
// void MergeMallocHeaders() {
//   IterMallocHeaders([](MallocHeader *header, void *) {
//     if (header->isUsed()) return true;
//
//     // Attempting to look past the end of the page. Stop here.
//     uintptr_t end = gKmallocStart + kMemLimit;
//     uintptr_t next = reinterpret_cast<uintptr_t>(header) +
//                      sizeof(MallocHeader) + header->getSize();
//     if (next >= end) return false;
//
//     // We can keep looping for sequential unused allocations.
//     MallocHeader *next_header = reinterpret_cast<MallocHeader *>(next);
//     while (next_header->getSize() == 0 || !next_header->isUsed()) {
//       header->IncSize(sizeof(MallocHeader) + next_header->getSize());
//
//       next = reinterpret_cast<uintptr_t>(header) + sizeof(MallocHeader) +
//              header->getSize();
//       if (next >= end) return false;
//
//       next_header = reinterpret_cast<MallocHeader *>(next);
//       assert(IsValid(header));
//     }
//     return true;
//   });
// }
//
//// Split an allocation for one malloc header into two allocations with
//// two headers. `new_size` indicates the desired size of the first partition.
//// Both headers are guaranteed to be valid after this.
// MallocHeader *Partition(MallocHeader *header, size_t desired_size,
//                         size_t desired_align) {
//   assert(!header->isUsed());
//   assert(IsValid(header));
//   uintptr_t addr = reinterpret_cast<uintptr_t>(header) +
//   sizeof(MallocHeader); uintptr_t end = addr + header->getSize();
//   assert(addr);
//   uintptr_t align = UINT32_C(1) << __builtin_ctz(addr);
//
//   if (header->getSize() == desired_size) {
//     if (align >= desired_align) return header;
//
//     // Can't possibly partition this to match alignment if the size is the
//     same. return nullptr;
//   }
//
//   if (header->getSize() < desired_size) return nullptr;
//
//   assert(header->getSize() - desired_size >= sizeof(MallocHeader));
//
//   if (align >= desired_align) {
//     // Do a normal split.
//     MallocHeader *new_header =
//         reinterpret_cast<MallocHeader *>(addr + desired_size);
//     new_header->setSize(header->getSize() - desired_size -
//                         sizeof(MallocHeader));
//     new_header->setUsed(0);
//     assert(IsValid(new_header));
//     assert(reinterpret_cast<uintptr_t>(new_header) + new_header->getSize() +
//                sizeof(MallocHeader) ==
//            end);
//
//     header->setSize(desired_size);
//     assert(IsValid(header));
//     assert(addr + header->getSize() ==
//     reinterpret_cast<uintptr_t>(new_header)); return header;
//   }
//
//   // See if we can create a partition in the middle of the current
//   allocation. uintptr_t new_addr = RoundUp(addr, desired_align);
//   assert(new_addr - addr >= sizeof(MallocHeader));
//   assert(new_addr % desired_align == 0);
//   if (new_addr + desired_size > end) return nullptr;
//
//   size_t new_size = end - new_addr;
//   if (!MallocHeader::SizeFits(new_size)) return nullptr;
//
//   MallocHeader *new_header =
//       reinterpret_cast<MallocHeader *>(new_addr - sizeof(MallocHeader));
//   assert(new_header != header);
//   new_header->setSize(new_size);
//   new_header->setUsed(0);
//   assert(IsValid(new_header));
//
//   header->setSize(new_addr - addr - sizeof(MallocHeader));
//   assert(IsValid(header));
//
//   // At this point, we could possibly shift and make a new second malloc
//   header.
//   // However, we can go further and partition that after re-aligning. To
//   handle
//   // that, we can recursively call this function after splitting off a
//   starting
//   // partition to align the new partition. Note that if we reach this point,
//   // then the recursive result *must* return something valid.
//   new_header = Partition(new_header, desired_size, desired_align);
//   assert(new_header);
//   assert(!new_header->isUsed());
//   assert(IsValid(new_header));
//   return new_header;
// };
//
// }  // namespace
//
// size_t GetAvailMemory() {
//   size_t avail = 0;
//   IterMallocHeaders(
//       [](MallocHeader *header, void *arg) {
//         if (!header->isUsed())
//           *reinterpret_cast<size_t *>(arg) += header->getSize();
//         return true;
//       },
//       &avail);
//   return avail;
// }
//
// void *kmalloc(size_t size) { return kmalloc(size, kMinAlign); }
//
// void *kmalloc(size_t size, size_t align) {
//   if (size == 0) return nullptr;
//
//   // Invalid alignment.
//   if (!IsPowerOf2(align)) return nullptr;
//
//   size = std::max(size, kMinSize);
//   size = RoundUp(size, kMinAlign);
//   align = std::max(align, kMinAlign);
//
//   // Cannot possibly allocate this.
//   if (size > kMemLimit) return nullptr;
//
//   // Cannot actually allocate this much since it will exceed the size limit
//   in
//   // the malloc header.
//   if (!MallocHeader::SizeFits(size)) return nullptr;
//
//   // Not enough memory left for use.
//   if (size > GetAvailMemory()) return nullptr;
//
//   struct Args {
//     const size_t desired_size;
//     const size_t desired_align;
//     MallocHeader *header;
//   };
//   Args args = {size, align, nullptr};
//   IterMallocHeaders(
//       [](MallocHeader *header, void *arg) {
//         if (header->isUsed()) return true;
//
//         Args *args = reinterpret_cast<Args *>(arg);
//         if (MallocHeader *free_header =
//                 Partition(header, args->desired_size, args->desired_align)) {
//           assert(!free_header->isUsed());
//           free_header->setUsed(1);
//           args->header = free_header;
//           return false;
//         }
//
//         // Keep searching.
//         return true;
//       },
//       &args);
//
//   MergeMallocHeaders();
//
//   if (!args.header) return nullptr;
//
//   uintptr_t header_addr = reinterpret_cast<uintptr_t>(args.header);
//   uintptr_t addr = header_addr + sizeof(MallocHeader);
//   assert(args.header->getSize() >= size);
//   assert(addr % align == 0);
//   assert(args.header->isUsed());
//
//   return reinterpret_cast<void *>(addr);
// }
//
// void kfree(void *ptr) {
//   if (!ptr) return;
//
//   assert(reinterpret_cast<uintptr_t>(ptr) % kMinAlign == 0 &&
//          "Expected all kmalloc'd pointers to be aligned.");
//
//   IterMallocHeaders(
//       [](MallocHeader *header, void *ptr) {
//         uintptr_t addr =
//             reinterpret_cast<uintptr_t>(header) + sizeof(MallocHeader);
//         if (addr == reinterpret_cast<uintptr_t>(ptr)) {
//           assert(header->isUsed() == 1 && "Attempting to free unused ptr");
//           header->setUsed(0);
//           return false;
//         }
//
//         return true;
//       },
//       ptr);
//
//   MallocHeader *header = reinterpret_cast<MallocHeader *>(ptr) - 1;
//   assert(!header->isUsed());
//
//   MergeMallocHeaders();
// }
//
// void DumpAllocs() {
//   printf("Headers:\n");
//   IterMallocHeaders([](MallocHeader *header, void *) {
//     printf("  addr: %p, size: %u, used: %d\n", header, header->getSize(),
//            header->isUsed());
//     return true;
//   });
// }

void Initialize() {
  // For simplicity, we will place all kernel allocations on a single 4MB page.
  // This means we will not be able to allocate anything greater than 4MB and
  // if we hit the limit, we're done.
  //
  // Allocate just one 4MB page to place all kernel allocations in. We can get
  // the virtual page that just comes after our kernel.
  uintptr_t kernel_begin = reinterpret_cast<uintptr_t>(&__KERNEL_BEGIN);
  uintptr_t kmalloc_vaddr = kernel_begin + pmm::kPageSize4M;
  assert(!paging::GetCurrentPageDirectory().VaddrIsMapped(kmalloc_vaddr));

  int32_t free_ppage = pmm::GetNextFreePage();
  assert(free_ppage >= 0 && "No free physical pages?");
  assert(!pmm::PageIsUsed(static_cast<uint32_t>(free_ppage)));
  pmm::SetPageUsed(static_cast<uint32_t>(free_ppage));
  paging::GetCurrentPageDirectory().MapPage(
      kmalloc_vaddr, static_cast<uint32_t>(free_ppage) * pmm::kPageSize4M,
      /*flags=*/0);

  libc::malloc::Initialize(/*alloc_start=*/kmalloc_vaddr,
                           /*alloc_size=*/pmm::kPageSize4M);
}

// void Initialize() {
//   // Allocate just one 4MB page to place all kernel allocations in. We can
//   get
//   // the virtual page that just comes after our kernel.
//   uintptr_t kernel_begin = reinterpret_cast<uintptr_t>(&__KERNEL_BEGIN);
//   uintptr_t kmalloc_vaddr = kernel_begin + pmm::kPageSize4M;
//   assert(!paging::GetCurrentPageDirectory().VaddrIsMapped(kmalloc_vaddr));
//
//   int32_t free_ppage = pmm::GetNextFreePage();
//   assert(free_ppage >= 0 && "No free physical pages?");
//   assert(!pmm::PageIsUsed(static_cast<uint32_t>(free_ppage)));
//   pmm::SetPageUsed(static_cast<uint32_t>(free_ppage));
//   paging::GetCurrentPageDirectory().MapPage(
//       kmalloc_vaddr, static_cast<uint32_t>(free_ppage) * pmm::kPageSize4M,
//       /*flags=*/0);
//
//   gKmallocStart = kmalloc_vaddr;
//
//   // Setup the first malloc header.
//   MallocHeader *head = reinterpret_cast<MallocHeader *>(gKmallocStart);
//   head->setSize(pmm::kPageSize4M - sizeof(MallocHeader));
//   head->setUsed(0);
//   assert(IsValid(head));
//   assert(GetAvailMemory() == pmm::kPageSize4M - sizeof(MallocHeader));
// }

}  // namespace kmalloc
