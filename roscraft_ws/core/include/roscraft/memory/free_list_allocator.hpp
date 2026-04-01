#pragma once

#include <roscraft/assert.hpp>
#include <roscraft/memory/aligned_alloc.hpp>
#include <roscraft/memory/common.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <mutex>
#include <new>

namespace roscraft::memory {

/// @brief Configuration for `FreeListAllocator`.
struct FreeListAllocatorOptions {
  size_t initial_capacity = 0;
  GrowthPolicy growth = GrowthPolicy::Fixed();
};

/**
 * @brief General-purpose PMR free-list allocator.
 * @details Uses a best-fit free-list with coalescing, protected by mutex.
 *
 * Hot path is optimized for minimal lock hold time, but allocator is not
 * lock-free due to variable-size block management and coalescing.
 */
class FreeListAllocator final : public std::pmr::memory_resource {
public:
  /**
   * @brief Constructs free-list allocator from options.
   * @warning Triggers assertion if initial capacity is too small.
   * @param options Allocator options
   */
  explicit FreeListAllocator(FreeListAllocatorOptions options) noexcept;

  /**
   * @brief Constructs free-list allocator with fixed growth.
   * @param initial_capacity Initial backing capacity
   */
  explicit FreeListAllocator(size_t initial_capacity) noexcept;

  FreeListAllocator(const FreeListAllocator&) = delete;
  FreeListAllocator(FreeListAllocator&& other) noexcept;
  ~FreeListAllocator() noexcept override;

  FreeListAllocator& operator=(const FreeListAllocator&) = delete;
  FreeListAllocator& operator=(FreeListAllocator&& other) noexcept;

  /**
   * @brief Resets allocator to initial state.
   * @warning Must not be called concurrently with allocate/deallocate.
   */
  void Reset() noexcept;

  /**
   * @brief Returns true when no allocations are active.
   * @return True if empty, false otherwise
   */
  [[nodiscard]] bool Empty() const noexcept {
    return allocation_count_.load(std::memory_order_relaxed) == 0;
  }

  /**
   * @brief Checks whether pointer belongs to allocator storage.
   * @param ptr Pointer to test
   * @return True when owned, false otherwise
   */
  [[nodiscard]] bool Owns(const void* ptr) const noexcept;

  /**
   * @brief Returns runtime stats.
   * @return Allocator stats snapshot
   */
  [[nodiscard]] AllocatorStats Stats() const noexcept;

  /**
   * @brief Returns total capacity across regions.
   * @return Capacity in bytes
   */
  [[nodiscard]] size_t Capacity() const noexcept {
    return capacity_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Returns used memory bytes.
   * @return Used bytes
   */
  [[nodiscard]] size_t UsedMemory() const noexcept {
    return used_memory_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Returns free memory bytes.
   * @return Free bytes
   */
  [[nodiscard]] size_t FreeMemory() const noexcept;

  /**
   * @brief Returns free-list block count.
   * @return Number of free blocks
   */
  [[nodiscard]] size_t FreeBlockCount() const noexcept {
    return free_block_count_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Returns current active allocation count.
   * @return Number of active allocations
   */
  [[nodiscard]] size_t AllocationCount() const noexcept {
    return allocation_count_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Returns growth policy.
   * @return Growth policy
   */
  [[nodiscard]] GrowthPolicy Growth() const noexcept { return growth_; }

private:
  struct FreeBlockHeader {
    size_t size = 0;
    FreeBlockHeader* next = nullptr;
  };

  struct AllocationHeader {
    size_t size = 0;
    size_t padding = 0;
  };

  struct RegionHeader {
    void* buffer = nullptr;
    size_t capacity = 0;
    std::atomic<RegionHeader*> next{nullptr};
  };

  static RegionHeader* CreateRegion(size_t capacity) noexcept;
  static void FreeRegions(RegionHeader* region) noexcept;
  static void AccumulatePeak(std::atomic<size_t>& peak,
                             size_t candidate) noexcept;

  [[nodiscard]] void* AllocateLocked(size_t bytes, size_t alignment) noexcept;
  void DeallocateLocked(void* ptr) noexcept;
  [[nodiscard]] bool GrowLocked(size_t min_capacity) noexcept;
  void InsertAndCoalesceLocked(FreeBlockHeader* block) noexcept;
  void InitializeRegionLocked(RegionHeader& region) noexcept;

  [[nodiscard]] void* do_allocate(size_t bytes, size_t alignment) override;
  void do_deallocate(void* p, size_t bytes, size_t alignment) override;
  [[nodiscard]] bool do_is_equal(
      const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }

  FreeBlockHeader* free_list_ = nullptr;
  std::atomic<RegionHeader*> regions_{nullptr};
  size_t initial_capacity_ = 0;
  GrowthPolicy growth_{};

  std::atomic<size_t> capacity_{0};
  std::atomic<size_t> used_memory_{0};
  std::atomic<size_t> peak_usage_{0};
  std::atomic<size_t> free_block_count_{0};
  std::atomic<size_t> allocation_count_{0};
  std::atomic<size_t> total_allocations_{0};
  std::atomic<size_t> total_deallocations_{0};
  std::atomic<size_t> alignment_waste_{0};

  mutable std::mutex mutex_;
};

inline FreeListAllocator::FreeListAllocator(
    FreeListAllocatorOptions options) noexcept
    : initial_capacity_(options.initial_capacity), growth_(options.growth) {
  ROSCRAFT_ASSERT(initial_capacity_ > sizeof(FreeBlockHeader),
                  "initial_capacity '{}' is too small", initial_capacity_);
  ROSCRAFT_ASSERT(growth_.max_capacity >= initial_capacity_,
                  "max_capacity '{}' must be >= initial_capacity '{}'",
                  growth_.max_capacity, initial_capacity_);

  RegionHeader* const initial_region = CreateRegion(initial_capacity_);
  ROSCRAFT_VERIFY(initial_region != nullptr,
                  "failed to allocate free-list region");

  regions_.store(initial_region, std::memory_order_release);
  capacity_.store(initial_capacity_, std::memory_order_relaxed);

  {
    const std::scoped_lock lock(mutex_);
    InitializeRegionLocked(*initial_region);
  }
}

inline FreeListAllocator::FreeListAllocator(size_t initial_capacity) noexcept
    : FreeListAllocator(FreeListAllocatorOptions{
          .initial_capacity = initial_capacity,
          .growth = GrowthPolicy::Fixed(),
      }) {}

inline FreeListAllocator::FreeListAllocator(FreeListAllocator&& other) noexcept
    : free_list_(other.free_list_),
      regions_(other.regions_.load(std::memory_order_acquire)),
      initial_capacity_(other.initial_capacity_),
      growth_(other.growth_),
      capacity_(other.capacity_.load(std::memory_order_acquire)),
      used_memory_(other.used_memory_.load(std::memory_order_acquire)),
      peak_usage_(other.peak_usage_.load(std::memory_order_acquire)),
      free_block_count_(
          other.free_block_count_.load(std::memory_order_acquire)),
      allocation_count_(
          other.allocation_count_.load(std::memory_order_acquire)),
      total_allocations_(
          other.total_allocations_.load(std::memory_order_acquire)),
      total_deallocations_(
          other.total_deallocations_.load(std::memory_order_acquire)),
      alignment_waste_(other.alignment_waste_.load(std::memory_order_acquire)) {
  other.free_list_ = nullptr;
  other.regions_.store(nullptr, std::memory_order_release);
  other.initial_capacity_ = 0;
  other.capacity_.store(0, std::memory_order_release);
  other.used_memory_.store(0, std::memory_order_release);
  other.peak_usage_.store(0, std::memory_order_release);
  other.free_block_count_.store(0, std::memory_order_release);
  other.allocation_count_.store(0, std::memory_order_release);
  other.total_allocations_.store(0, std::memory_order_release);
  other.total_deallocations_.store(0, std::memory_order_release);
  other.alignment_waste_.store(0, std::memory_order_release);
}

inline FreeListAllocator::~FreeListAllocator() noexcept {
  FreeRegions(regions_.load(std::memory_order_acquire));
}

inline FreeListAllocator& FreeListAllocator::operator=(
    FreeListAllocator&& other) noexcept {
  if (this == &other) [[unlikely]] {
    return *this;
  }

  {
    const std::scoped_lock lock(mutex_, other.mutex_);
    FreeRegions(regions_.load(std::memory_order_acquire));
    free_list_ = other.free_list_;
    regions_.store(other.regions_.load(std::memory_order_acquire),
                   std::memory_order_release);
    other.free_list_ = nullptr;
    other.regions_.store(nullptr, std::memory_order_release);
  }

  initial_capacity_ = other.initial_capacity_;
  growth_ = other.growth_;
  capacity_.store(other.capacity_.load(std::memory_order_acquire),
                  std::memory_order_release);
  used_memory_.store(other.used_memory_.load(std::memory_order_acquire),
                     std::memory_order_release);
  peak_usage_.store(other.peak_usage_.load(std::memory_order_acquire),
                    std::memory_order_release);
  free_block_count_.store(
      other.free_block_count_.load(std::memory_order_acquire),
      std::memory_order_release);
  allocation_count_.store(
      other.allocation_count_.load(std::memory_order_acquire),
      std::memory_order_release);
  total_allocations_.store(
      other.total_allocations_.load(std::memory_order_acquire),
      std::memory_order_release);
  total_deallocations_.store(
      other.total_deallocations_.load(std::memory_order_acquire),
      std::memory_order_release);
  alignment_waste_.store(other.alignment_waste_.load(std::memory_order_acquire),
                         std::memory_order_release);

  other.initial_capacity_ = 0;
  other.capacity_.store(0, std::memory_order_release);
  other.used_memory_.store(0, std::memory_order_release);
  other.peak_usage_.store(0, std::memory_order_release);
  other.free_block_count_.store(0, std::memory_order_release);
  other.allocation_count_.store(0, std::memory_order_release);
  other.total_allocations_.store(0, std::memory_order_release);
  other.total_deallocations_.store(0, std::memory_order_release);
  other.alignment_waste_.store(0, std::memory_order_release);

  return *this;
}

inline void FreeListAllocator::Reset() noexcept {
  const std::scoped_lock lock(mutex_);
  free_list_ = nullptr;
  free_block_count_.store(0, std::memory_order_relaxed);

  RegionHeader* region = regions_.load(std::memory_order_acquire);
  while (region != nullptr) {
    InitializeRegionLocked(*region);
    region = region->next.load(std::memory_order_acquire);
  }

  used_memory_.store(0, std::memory_order_release);
  allocation_count_.store(0, std::memory_order_release);
  total_allocations_.store(0, std::memory_order_release);
  total_deallocations_.store(0, std::memory_order_release);
  alignment_waste_.store(0, std::memory_order_release);
}

inline bool FreeListAllocator::Owns(const void* ptr) const noexcept {
  if (ptr == nullptr) [[unlikely]] {
    return false;
  }

  const auto addr = reinterpret_cast<uintptr_t>(ptr);
  RegionHeader* region = regions_.load(std::memory_order_acquire);
  while (region != nullptr) {
    const auto begin = reinterpret_cast<uintptr_t>(region->buffer);
    const auto end = begin + region->capacity;
    if (addr >= begin && addr < end) {
      return true;
    }
    region = region->next.load(std::memory_order_acquire);
  }

  return false;
}

inline AllocatorStats FreeListAllocator::Stats() const noexcept {
  return {
      .total_allocated = used_memory_.load(std::memory_order_relaxed),
      .peak_usage = peak_usage_.load(std::memory_order_relaxed),
      .allocation_count = allocation_count_.load(std::memory_order_relaxed),
      .total_allocations = total_allocations_.load(std::memory_order_relaxed),
      .total_deallocations =
          total_deallocations_.load(std::memory_order_relaxed),
      .alignment_waste = alignment_waste_.load(std::memory_order_relaxed),
  };
}

inline size_t FreeListAllocator::FreeMemory() const noexcept {
  const size_t cap = capacity_.load(std::memory_order_relaxed);
  const size_t used = used_memory_.load(std::memory_order_relaxed);
  return cap >= used ? cap - used : 0;
}

inline auto FreeListAllocator::CreateRegion(size_t capacity) noexcept
    -> RegionHeader* {
  constexpr size_t kAlign =
      std::max({alignof(RegionHeader), kDefaultAlignment, kMinAlignment});
  const size_t header_size = AlignUp(sizeof(RegionHeader), kAlign);
  const size_t total_size = SaturatingAdd(header_size, capacity);

  void* const raw = AlignedAlloc(kAlign, total_size);
  if (raw == nullptr) [[unlikely]] {
    return nullptr;
  }

  auto* const region = std::construct_at(static_cast<RegionHeader*>(raw));
  region->buffer = static_cast<std::byte*>(raw) + header_size;
  region->capacity = capacity;
  region->next.store(nullptr, std::memory_order_relaxed);
  return region;
}

inline void FreeListAllocator::FreeRegions(RegionHeader* region) noexcept {
  RegionHeader* current = region;
  while (current != nullptr) {
    RegionHeader* const next = current->next.load(std::memory_order_relaxed);
    std::destroy_at(current);
    AlignedFree(current);
    current = next;
  }
}

inline void FreeListAllocator::AccumulatePeak(std::atomic<size_t>& peak,
                                              size_t candidate) noexcept {
  size_t observed = peak.load(std::memory_order_relaxed);
  while (candidate > observed) {
    if (peak.compare_exchange_weak(observed, candidate,
                                   std::memory_order_relaxed,
                                   std::memory_order_relaxed)) {
      return;
    }
  }
}

inline void* FreeListAllocator::AllocateLocked(size_t bytes,
                                               size_t alignment) noexcept {
  constexpr size_t kHeaderSize = sizeof(AllocationHeader);
  const size_t effective_alignment = std::max(alignment, kMinAlignment);
  const size_t storage_alignment =
      std::max(effective_alignment, alignof(AllocationHeader));

  FreeBlockHeader* best = nullptr;
  FreeBlockHeader* best_prev = nullptr;
  size_t best_padding = 0;
  size_t best_total = std::numeric_limits<size_t>::max();

  FreeBlockHeader* current = free_list_;
  FreeBlockHeader* prev = nullptr;

  while (current != nullptr) {
    auto* const block_start = reinterpret_cast<std::byte*>(current);
    const size_t padding =
        CalculatePaddingWithHeader(block_start, storage_alignment, kHeaderSize);
    const size_t total = SaturatingAdd(padding, bytes);

    if (current->size >= total && total < best_total) {
      best = current;
      best_prev = prev;
      best_padding = padding;
      best_total = total;
      if (current->size == total) {
        break;
      }
    }

    prev = current;
    current = current->next;
  }

  if (best == nullptr) [[unlikely]] {
    return nullptr;
  }

  if (best_prev != nullptr) {
    best_prev->next = best->next;
  } else {
    free_list_ = best->next;
  }
  free_block_count_.fetch_sub(1, std::memory_order_relaxed);

  const size_t remaining = best->size - best_total;
  constexpr size_t kMinSplit =
      sizeof(FreeBlockHeader) + alignof(FreeBlockHeader);
  if (remaining >= kMinSplit) {
    auto* const split_raw = reinterpret_cast<std::byte*>(best) + best_total;
    const size_t split_pad =
        CalculatePadding(split_raw, alignof(FreeBlockHeader));
    auto* const split_block =
        std::launder(reinterpret_cast<FreeBlockHeader*>(split_raw + split_pad));
    const size_t split_size =
        remaining >= split_pad ? remaining - split_pad : 0;
    if (split_size >= sizeof(FreeBlockHeader) + 1) {
      split_block->size = split_size;
      split_block->next = nullptr;
      InsertAndCoalesceLocked(split_block);
    }
  }

  auto* const user_ptr = reinterpret_cast<std::byte*>(best) + best_padding;
  auto* const header =
      std::launder(reinterpret_cast<AllocationHeader*>(user_ptr - kHeaderSize));
  header->size = best_total;
  header->padding = best_padding;
  alignment_waste_.fetch_add(
      best_padding > kHeaderSize ? best_padding - kHeaderSize : 0,
      std::memory_order_relaxed);
  return user_ptr;
}

inline void FreeListAllocator::DeallocateLocked(void* ptr) noexcept {
  auto* const header = std::launder(reinterpret_cast<AllocationHeader*>(
      static_cast<std::byte*>(ptr) - sizeof(AllocationHeader)));
  auto* const block_begin = static_cast<std::byte*>(ptr) - header->padding;
  auto* const free_block =
      std::launder(reinterpret_cast<FreeBlockHeader*>(block_begin));
  free_block->size = header->size;
  free_block->next = nullptr;
  InsertAndCoalesceLocked(free_block);
}

inline bool FreeListAllocator::GrowLocked(size_t min_capacity) noexcept {
  const size_t current_capacity = capacity_.load(std::memory_order_relaxed);
  const size_t next_capacity = growth_.NextCapacity(
      current_capacity, SaturatingAdd(current_capacity, min_capacity));
  if (next_capacity <= current_capacity) {
    return false;
  }

  const size_t region_capacity = next_capacity - current_capacity;
  RegionHeader* const region = CreateRegion(region_capacity);
  if (region == nullptr) [[unlikely]] {
    return false;
  }

  RegionHeader* observed = regions_.load(std::memory_order_acquire);
  do {
    region->next.store(observed, std::memory_order_relaxed);
  } while (!regions_.compare_exchange_weak(
      observed, region, std::memory_order_release, std::memory_order_acquire));

  capacity_.fetch_add(region_capacity, std::memory_order_relaxed);
  InitializeRegionLocked(*region);
  return true;
}

inline void FreeListAllocator::InsertAndCoalesceLocked(
    FreeBlockHeader* block) noexcept {
  const auto block_addr = reinterpret_cast<uintptr_t>(block);
  FreeBlockHeader* prev = nullptr;
  FreeBlockHeader* current = free_list_;

  while (current != nullptr &&
         reinterpret_cast<uintptr_t>(current) < block_addr) {
    prev = current;
    current = current->next;
  }

  block->next = current;
  if (prev != nullptr) {
    prev->next = block;
  } else {
    free_list_ = block;
  }
  free_block_count_.fetch_add(1, std::memory_order_relaxed);

  if (current != nullptr) {
    auto* const block_end = reinterpret_cast<std::byte*>(block) + block->size;
    if (block_end == reinterpret_cast<std::byte*>(current)) {
      block->size += current->size;
      block->next = current->next;
      free_block_count_.fetch_sub(1, std::memory_order_relaxed);
    }
  }

  if (prev != nullptr) {
    auto* const prev_end = reinterpret_cast<std::byte*>(prev) + prev->size;
    if (prev_end == reinterpret_cast<std::byte*>(block)) {
      prev->size += block->size;
      prev->next = block->next;
      free_block_count_.fetch_sub(1, std::memory_order_relaxed);
    }
  }
}

inline void FreeListAllocator::InitializeRegionLocked(
    RegionHeader& region) noexcept {
  auto* const block = static_cast<FreeBlockHeader*>(region.buffer);
  block->size = region.capacity;
  block->next = nullptr;
  InsertAndCoalesceLocked(block);
}

inline void* FreeListAllocator::do_allocate(size_t bytes, size_t alignment) {
  if (bytes == 0) [[unlikely]] {
    return nullptr;
  }

  if (!IsPowerOfTwo(alignment)) [[unlikely]] {
    PmrFailFast("alignment is not a power of two");
  }

  void* result = nullptr;
  {
    const std::scoped_lock lock(mutex_);
    result = AllocateLocked(bytes, alignment);
    if (result == nullptr) [[unlikely]] {
      if (!GrowLocked(SaturatingAdd(bytes, alignment))) [[unlikely]] {
        PmrFailFast("free-list allocator out of memory and cannot grow");
      }
      result = AllocateLocked(bytes, alignment);
      if (result == nullptr) [[unlikely]] {
        PmrFailFast("free-list allocator allocation failed after growth");
      }
    }
  }

  const auto* header = std::launder(reinterpret_cast<const AllocationHeader*>(
      static_cast<const std::byte*>(result) - sizeof(AllocationHeader)));
  const size_t consumed = header->size;

  const size_t used =
      used_memory_.fetch_add(consumed, std::memory_order_relaxed) + consumed;
  AccumulatePeak(peak_usage_, used);
  allocation_count_.fetch_add(1, std::memory_order_relaxed);
  total_allocations_.fetch_add(1, std::memory_order_relaxed);
  return result;
}

inline void FreeListAllocator::do_deallocate(void* p, size_t /*bytes*/,
                                             size_t /*alignment*/) {
  if (p == nullptr) [[unlikely]] {
    return;
  }

  if (!Owns(p)) [[unlikely]] {
    ROSCRAFT_ASSERT(false, "pointer does not belong to free-list allocator");
    return;
  }

  size_t consumed = 0;
  {
    const std::scoped_lock lock(mutex_);
    const auto* header = std::launder(reinterpret_cast<const AllocationHeader*>(
        static_cast<const std::byte*>(p) - sizeof(AllocationHeader)));
    consumed = header->size;
    DeallocateLocked(p);
  }

  used_memory_.fetch_sub(consumed, std::memory_order_relaxed);
  allocation_count_.fetch_sub(1, std::memory_order_relaxed);
  total_deallocations_.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace roscraft::memory
