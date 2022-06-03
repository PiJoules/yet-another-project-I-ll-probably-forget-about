#include <assert.h>
#include <libc/malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>

#define MALLOC_VALIDATION 1

#ifdef NDEBUG
#define DEBUG_PRINT(...) (void)(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) printf("DEBUG_PRINT> " __VA_ARGS__)
#endif

#ifdef __KERNEL__
#include <kernel/kernel.h>
#include <kernel/paging.h>
#include <kernel/pmm.h>
#else  // __KERNEL__

#include <type_traits>

namespace {

template <typename T>
constexpr inline bool IsPowerOf2(T x) {
  return x != 0 && ((x & (x - 1)) == 0);
}

template <size_t Align, typename T,
          std::enable_if_t<IsPowerOf2<T>(Align), bool> = true>
constexpr inline T RoundUp(T x) {
  return (x + (Align - 1)) & ~(Align - 1);
}

template <typename T>
constexpr inline T RoundUp(T x, size_t align) {
  assert(IsPowerOf2<T>(align));
  return (x + (static_cast<T>(align) - 1)) & ~(static_cast<T>(align) - 1);
}

}  // namespace

#endif  // __KERNEL__

extern "C" uint32_t __KERNEL_BEGIN;

namespace libc {
namespace malloc {
namespace {

// These are set in `Initialize`.
size_t gAllocSize;
uintptr_t gMallocStart;

// `MallocHeader` is a shadow allocation placed right before the pointer we
// return from `malloc`.
constexpr size_t kMinSize = 4;
constexpr size_t kSizeBitsMax = 31;
constexpr size_t kSizeMax = (UINT32_C(1) << kSizeBitsMax) - 1;
// constexpr size_t kSizeMax = UINT32_MAX;
//  Manually set alignment here because it's possible for alignof(MallocHeader)
//  to be 1 (without alignas) since it's packed.
struct alignas(kMinSize) MallocHeader {
  // This refers to the size of the allocated memory following this header.
  // Note that since the malloc headers need to be aligned, the sizes must also
  // be a multiple of the malloc header size.
  void setSize(size_t size) {
    assert(SizeFits(size));
    assert(size % kMinSize == 0);
    assert(size);
    size_ = size;
  }

  void IncSize(size_t inc) {
    assert(SizeFits(size_, inc));
    size_ += inc;
    assert(size_);
  }

  size_t getSize() const { return size_; }

  void setUsed(bool used) { used_ = used; }

  bool isUsed() const { return used_; }

  static bool SizeFits(size_t s) { return s <= kSizeMax; }
  static bool SizeFits(size_t s, size_t inc) {
    if (s + inc < s) return false;
    return s + inc <= kSizeMax;
  }

 private:
  uint32_t size_ : kSizeBitsMax;
  uint32_t used_ : 1;
} __attribute__((packed));
static_assert(sizeof(MallocHeader) == kMinSize);
static_assert(alignof(MallocHeader) == kMinSize);

constexpr size_t kMinAlign = kMinSize;

// All malloc headers are aligned to 4 bytes. This also implies the size must
// be a multiple of 4 bytes. Note this also allows zero-length allocations to
// pass, but those should be cleaned up whenever we exit a function here.
bool IsValid(const MallocHeader *header) {
  return (reinterpret_cast<uintptr_t>(header) % kMinAlign == 0 &&
          header->getSize() % kMinAlign == 0);
}

// Return false to stop iterating.
using malloc_header_callback_t = bool (*)(MallocHeader *header, void *arg);

void IterMallocHeaders(malloc_header_callback_t callback, void *arg = nullptr) {
  uintptr_t start = gMallocStart;
  assert(reinterpret_cast<MallocHeader *>(start)->getSize());
  uintptr_t end = start + gAllocSize;
  while (start < end) {
    if (!callback(reinterpret_cast<MallocHeader *>(start), arg)) return;
    // Note that because we dereference the ptr again here, we can edit the
    // header in place and jump safely to the next header.
    start += sizeof(MallocHeader) +
             reinterpret_cast<MallocHeader *>(start)->getSize();
  }
}

// We can merge two adjascent malloc headers if they meet any of the following
// criteria:
// - The current one is free and the next one is size zero.
// - The current one and the next one are free.
void MergeMallocHeaders() {
  IterMallocHeaders([](MallocHeader *header, void *) {
    if (header->isUsed()) return true;

    // Attempting to look past the end of the page. Stop here.
    uintptr_t end = gMallocStart + gAllocSize;
    uintptr_t next = reinterpret_cast<uintptr_t>(header) +
                     sizeof(MallocHeader) + header->getSize();
    if (next >= end) return false;

    // We can keep looping for sequential unused allocations.
    MallocHeader *next_header = reinterpret_cast<MallocHeader *>(next);
    while (next_header->getSize() == 0 || !next_header->isUsed()) {
      header->IncSize(sizeof(MallocHeader) + next_header->getSize());

      next = reinterpret_cast<uintptr_t>(header) + sizeof(MallocHeader) +
             header->getSize();
      if (next >= end) return false;

      next_header = reinterpret_cast<MallocHeader *>(next);
      assert(IsValid(header));
    }
    return true;
  });
}

// Split an allocation for one malloc header into two allocations with
// two headers. `new_size` indicates the desired size of the first partition.
// Both headers are guaranteed to be valid after this.
MallocHeader *Partition(MallocHeader *header, const size_t desired_size,
                        const size_t desired_align) {
  assert(!header->isUsed());
  assert(IsValid(header));
  uintptr_t addr = reinterpret_cast<uintptr_t>(header) + sizeof(MallocHeader);
  uintptr_t end = addr + header->getSize();
  assert(addr);
  uintptr_t align = UINT32_C(1) << __builtin_ctz(addr);

  if (header->getSize() == desired_size) {
    if (align >= desired_align) return header;

    // Can't possibly partition this to match alignment if the size is the same.
    return nullptr;
  }

  if (header->getSize() < desired_size) return nullptr;

  assert(header->getSize() - desired_size >= sizeof(MallocHeader));

  if (align >= desired_align) {
    // For this check, we know that the address for this header is aligned,
    // and we will need to form a partition, since the header size at this
    // point is greater than the `desired_size`. We cannot form a reliable
    // second partition though if the remaining space left is zero.
    if (header->getSize() - desired_size == sizeof(MallocHeader))
      return nullptr;

    // This partition is "normal" in that we create the partition from the start
    // of the header rather than somewhere in the middle.
    MallocHeader *new_header =
        reinterpret_cast<MallocHeader *>(addr + desired_size);
    new_header->setSize(header->getSize() - desired_size -
                        sizeof(MallocHeader));
    new_header->setUsed(0);
    assert(IsValid(new_header));
    assert(reinterpret_cast<uintptr_t>(new_header) + new_header->getSize() +
               sizeof(MallocHeader) ==
           end);

    header->setSize(desired_size);
    assert(IsValid(header));
    assert(addr + header->getSize() == reinterpret_cast<uintptr_t>(new_header));
    return header;
  }

  // See if we can create a partition somewhere in the current allocation.
  uintptr_t new_addr = RoundUp(addr, desired_align);
  assert(new_addr - addr >= sizeof(MallocHeader));
  assert(new_addr % desired_align == 0);

  if (new_addr - addr == sizeof(MallocHeader)) {
    // This indicates that, within this malloc header, we found an address that
    // meets our alignment constraint, but if we were to partition from this
    // new address, the first header in the partition would only have a size of
    // zero. This is because a malloc header must be placed before this
    // `new_addr`, meaning it will be placed *at* `addr` immediately after the
    // first malloc header.
    //
    // An example of this is a `malloc(1, 8)` and we come upon this header:
    //
    //   header addr: 0x00802468, size: 4184980, used: 0
    //
    // A valid `new_addr` would be 0x00802470, meaning the new malloc header
    // would be at 0x0080246c, but that would mean this header would need size
    // of zero, which we don't allow. In this case, what we can do is check if
    // the next aligned address is within this malloc header and if we can
    // partion that.
    new_addr += desired_align;
  }

  if (new_addr + desired_size > end) return nullptr;

  size_t new_size = end - new_addr;
  if (!MallocHeader::SizeFits(new_size)) return nullptr;
  if (new_size == 0) return nullptr;

  MallocHeader *new_header =
      reinterpret_cast<MallocHeader *>(new_addr - sizeof(MallocHeader));
  assert(new_header != header);
  new_header->setSize(new_size);
  new_header->setUsed(0);
  assert(IsValid(new_header));

  // Note that this size *must* be greater than zero given the assertions above.
  header->setSize(new_addr - addr - sizeof(MallocHeader));
  assert(IsValid(header));

  // At this point, we could possibly shift and make a new second malloc header.
  // However, we can go further and partition that after re-aligning. To handle
  // that, we can recursively call this function after splitting off a starting
  // partition to align the new partition. Note that if we reach this point,
  // then the recursive result *must* return something valid.
  new_header = Partition(new_header, desired_size, desired_align);
  assert(new_header);
  assert(!new_header->isUsed());
  assert(IsValid(new_header));
  return new_header;
};

}  // namespace

size_t GetAvailMemory() {
  size_t avail = 0;
  IterMallocHeaders(
      [](MallocHeader *header, void *arg) {
        if (!header->isUsed())
          *reinterpret_cast<size_t *>(arg) += header->getSize();
        return true;
      },
      &avail);
  return avail;
}

void *malloc(size_t size) { return malloc(size, kMinAlign); }

static void *MallocImpl(size_t size, size_t align) {
  if (size == 0) return nullptr;

  // Invalid alignment.
  if (!IsPowerOf2(align)) return nullptr;

  size = std::max(size, kMinSize);
  size = RoundUp(size, kMinAlign);
  align = std::max(align, kMinAlign);

  // Cannot possibly allocate this.
  if (size > gAllocSize) return nullptr;

  // Cannot actually allocate this much since it will exceed the size limit in
  // the malloc header.
  if (!MallocHeader::SizeFits(size)) return nullptr;

  // Not enough memory left for use.
  if (size > GetAvailMemory()) return nullptr;

  struct Args {
    const size_t desired_size;
    const size_t desired_align;
    MallocHeader *header;
  };
  Args args = {size, align, nullptr};
  IterMallocHeaders(
      [](MallocHeader *header, void *arg) {
        if (header->isUsed()) return true;

        Args *args = reinterpret_cast<Args *>(arg);
        if (MallocHeader *free_header =
                Partition(header, args->desired_size, args->desired_align)) {
          assert(!free_header->isUsed());
          free_header->setUsed(1);
          args->header = free_header;
          return false;
        }

        // Keep searching.
        return true;
      },
      &args);

  MergeMallocHeaders();

  if (!args.header) return nullptr;

  uintptr_t header_addr = reinterpret_cast<uintptr_t>(args.header);
  uintptr_t addr = header_addr + sizeof(MallocHeader);
  assert(args.header->getSize() >= size);
  assert(addr % align == 0);
  assert(args.header->isUsed());

  return reinterpret_cast<void *>(addr);
}

void *malloc(size_t size, size_t align) {
  void *res = MallocImpl(size, align);
  if (!res) {
    DEBUG_PRINT("MALLOC RETURNED NULL! size: %u, align: %u\n", size, align);
    DumpAllocs();
  }

#if MALLOC_VALIDATION
  struct ValidateParams {
    size_t size;
    size_t align;
    void *res;
  } params = {size, align, res};
  IterMallocHeaders(
      [](MallocHeader *header, void *arg) {
        if (header->getSize() == 0) {
          auto *param = reinterpret_cast<ValidateParams *>(arg);
          DEBUG_PRINT(
              "FOUND A HEADER WITH ZERO SIZE AT %p! THIS SHOULD NOT HAPPEN.\n",
              header);
          DEBUG_PRINT("This was during a malloc(%u, %u) that returned %p\n",
                      param->size, param->align, param->res);
          DumpAllocs();
          abort();
        }
        return true;
      },
      &params);
#endif

  return res;
}

void free(void *ptr) {
  if (!ptr) return;

  assert(reinterpret_cast<uintptr_t>(ptr) % kMinAlign == 0 &&
         "Expected all malloc'd pointers to be aligned.");

#if MALLOC_VALIDATION
  MallocHeader saved_header = *(reinterpret_cast<MallocHeader *>(ptr) - 1);
#endif

  struct Params {
    void *ptr;
    bool found_ptr;
  } params = {ptr, false};

  IterMallocHeaders(
      [](MallocHeader *header, void *arg) {
        auto *params = reinterpret_cast<Params *>(arg);
        uintptr_t addr =
            reinterpret_cast<uintptr_t>(header) + sizeof(MallocHeader);
        if (addr == reinterpret_cast<uintptr_t>(params->ptr)) {
          assert(header->isUsed() == 1 && "Attempting to free unused ptr");
          header->setUsed(0);
          params->found_ptr = true;
          return false;
        }

        return true;
      },
      &params);

  assert(params.found_ptr && "Didn't find the pointer we want to free!");

  MallocHeader *header = reinterpret_cast<MallocHeader *>(ptr) - 1;
  assert(!header->isUsed());

  MergeMallocHeaders();

#if MALLOC_VALIDATION
  struct ValidateParams {
    MallocHeader *saved_header;
    void *free_ptr;
  } validate_params = {&saved_header, ptr};
  IterMallocHeaders(
      [](MallocHeader *header, void *arg) {
        if (header->getSize() == 0) {
          auto *param = reinterpret_cast<ValidateParams *>(arg);
          DEBUG_PRINT(
              "FOUND A HEADER WITH ZERO SIZE AT %p! THIS SHOULD NOT HAPPEN.\n",
              header);
          DEBUG_PRINT(
              "This was during a free(%p) from a header with size: %u\n",
              param->free_ptr, param->saved_header->getSize());
          DumpAllocs();
          abort();
        }
        return true;
      },
      &validate_params);
#endif
}

void DumpAllocs() {
  DEBUG_PRINT("Headers:\n");
  IterMallocHeaders([](MallocHeader *header, void *) {
    DEBUG_PRINT("  addr: %p, size: %u, used: %d\n", header, header->getSize(),
                header->isUsed());
    return true;
  });
}

void Initialize(uintptr_t alloc_start, size_t alloc_size) {
  gAllocSize = alloc_size;
  gMallocStart = alloc_start;
  DEBUG_PRINT("Malloc start at 0x%x\n", alloc_start);

  // Setup the first malloc header.
  MallocHeader *head = reinterpret_cast<MallocHeader *>(gMallocStart);
  head->setSize(alloc_size - sizeof(MallocHeader));
  head->setUsed(0);
  assert(IsValid(head));
  assert(GetAvailMemory() == alloc_size - sizeof(MallocHeader));
}

}  // namespace malloc

}  // namespace libc