#pragma once

#include <roscraft/assert.hpp>
#include <roscraft/memory/aligned_alloc.hpp>
#include <roscraft/memory/common.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>

namespace roscraft::memory {

/// @brief Configuration for StackAllocator.
struct StackAllocatorOptions {
  size_t initial_capacity = 0;
  GrowthPolicy growth = GrowthPolicy::Fixed();
};

/**
 * @brief PMR stack allocator with LIFO-aware deallocation.
 * @details Uses block-chained bump allocation with per-allocation headers.
 *
 * Allocation path is lock-free with atomic CAS on head block offset. Deallocate
 * tries LIFO rewind when possible. Non-LIFO deallocation is treated as no-op,
 * which keeps PMR compatibility for containers that may free out of order.
 *
 * @note Thread-safe for `allocate`. `Reset()` and marker operations require
 * external synchronization.
 */
class StackAllocator final : public std::pmr::memory_resource {
public:
  /// @brief Marker for rewind operations.
  struct Marker {
    void* block = nullptr;
    size_t offset = 0;
  };

  /**
   * @brief Constructs stack allocator from options.
   * @warning Triggers assertion if `options.initial_capacity == 0`.
   * @param options Stack allocator options
   */
  explicit StackAllocator(StackAllocatorOptions options) noexcept;

  /**
   * @brief Constructs stack allocator with fixed growth.
   * @warning Triggers assertion if `initial_capacity == 0`.
   * @param initial_capacity Initial block capacity
   */
  explicit StackAllocator(size_t initial_capacity) noexcept;

  StackAllocator(const StackAllocator&) = delete;
  StackAllocator(StackAllocator&& other) noexcept;
  ~StackAllocator() noexcept override;

  auto operator=(const StackAllocator&) -> StackAllocator& = delete;
  auto operator=(StackAllocator&& other) noexcept -> StackAllocator&;

  /**
   * @brief Returns current marker.
   * @return Marker that can later be rewound to
   */
  [[nodiscard]] Marker GetMarker() const noexcept;

  /**
   * @brief Rewinds allocator to marker.
   * @warning Must not be called concurrently with allocation/deallocation.
   * Triggers assertion if marker does not belong to this allocator.
   * @param marker Marker previously returned by `GetMarker`
   */
  void RewindToMarker(Marker marker) noexcept;

  /**
   * @brief Resets allocator to a fresh single initial block.
   * @warning Must not be called concurrently with allocation/deallocation.
   */
  void Reset() noexcept;

  /**
   * @brief Returns true when no active allocations remain.
   * @return True if empty, false otherwise.
   */
  [[nodiscard]] bool Empty() const noexcept {
    return allocation_count_.load(std::memory_order_relaxed) == 0;
  }

  /**
   * @brief Returns runtime statistics.
   * @return Allocator stats snapshot
   */
  [[nodiscard]] AllocatorStats Stats() const noexcept;

  /**
   * @brief Returns initial block capacity.
   * @return Capacity in bytes
   */
  [[nodiscard]] size_t InitialCapacity() const noexcept {
    return initial_capacity_;
  }

  /**
   * @brief Returns total capacity across block chain.
   * @return Capacity in bytes
   */
  [[nodiscard]] size_t TotalCapacity() const noexcept {
    return total_capacity_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Returns current number of blocks.
   * @return Block count
   */
  [[nodiscard]] size_t BlockCount() const noexcept {
    return block_count_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Returns growth policy.
   * @return Growth policy
   */
  [[nodiscard]] GrowthPolicy Growth() const noexcept { return growth_; }

private:
  struct AllocationHeader {
    size_t previous_offset = 0;
    size_t total_size = 0;
  };

  struct Block {
    void* buffer = nullptr;
    size_t capacity = 0;
    std::atomic<size_t> offset{0};
    std::atomic<Block*> next{nullptr};
  };

  struct Reservation {
    void* ptr = nullptr;
    size_t allocation_size = 0;
    size_t consumed = 0;
    size_t previous_offset = 0;
  };

  static constexpr uint8_t kGrowStateIdle = 0;
  static constexpr uint8_t kGrowStateGrowing = 1;

  [[nodiscard]] static Block* CreateBlock(size_t capacity) noexcept;
  static void FreeChain(Block* head) noexcept;
  [[nodiscard]] static Reservation TryReserve(Block& block, size_t size,
                                              size_t alignment) noexcept;
  [[nodiscard]] bool EnsureCapacity(size_t min_capacity) noexcept;
  void PublishBlock(Block* block) noexcept;
  static void AccumulatePeak(std::atomic<size_t>& peak,
                             size_t candidate) noexcept;

  [[nodiscard]] void* do_allocate(size_t bytes, size_t alignment) override;
  void do_deallocate(void* p, size_t bytes, size_t alignment) override;
  [[nodiscard]] bool do_is_equal(
      const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }

  std::atomic<Block*> head_{nullptr};
  std::atomic<uint8_t> grow_state_{kGrowStateIdle};
  size_t initial_capacity_ = 0;
  GrowthPolicy growth_{};

  std::atomic<size_t> total_capacity_{0};
  std::atomic<size_t> total_allocated_{0};
  std::atomic<size_t> peak_usage_{0};
  std::atomic<size_t> allocation_count_{0};
  std::atomic<size_t> total_allocations_{0};
  std::atomic<size_t> total_deallocations_{0};
  std::atomic<size_t> alignment_waste_{0};
  std::atomic<size_t> block_count_{0};
};

inline StackAllocator::StackAllocator(StackAllocatorOptions options) noexcept
    : initial_capacity_(options.initial_capacity), growth_(options.growth) {
  ROSCRAFT_ASSERT(initial_capacity_ > 0,
                  "initial_capacity must be greater than zero");
  ROSCRAFT_ASSERT(growth_.max_capacity >= initial_capacity_,
                  "max_capacity '{}' must be >= initial_capacity '{}'",
                  growth_.max_capacity, initial_capacity_);

  Block* const initial_block = CreateBlock(initial_capacity_);
  ROSCRAFT_VERIFY(initial_block != nullptr, "failed to allocate initial block");
  head_.store(initial_block, std::memory_order_release);
  total_capacity_.store(initial_capacity_, std::memory_order_relaxed);
  block_count_.store(1, std::memory_order_relaxed);
}

inline StackAllocator::StackAllocator(size_t initial_capacity) noexcept
    : StackAllocator(StackAllocatorOptions{.initial_capacity = initial_capacity,
                                           .growth = GrowthPolicy::Fixed()}) {}

inline StackAllocator::StackAllocator(StackAllocator&& other) noexcept
    : head_(other.head_.load(std::memory_order_acquire)),
      grow_state_(other.grow_state_.load(std::memory_order_acquire)),
      initial_capacity_(other.initial_capacity_),
      growth_(other.growth_),
      total_capacity_(other.total_capacity_.load(std::memory_order_acquire)),
      total_allocated_(other.total_allocated_.load(std::memory_order_acquire)),
      peak_usage_(other.peak_usage_.load(std::memory_order_acquire)),
      allocation_count_(
          other.allocation_count_.load(std::memory_order_acquire)),
      total_allocations_(
          other.total_allocations_.load(std::memory_order_acquire)),
      total_deallocations_(
          other.total_deallocations_.load(std::memory_order_acquire)),
      alignment_waste_(other.alignment_waste_.load(std::memory_order_acquire)),
      block_count_(other.block_count_.load(std::memory_order_acquire)) {
  other.head_.store(nullptr, std::memory_order_release);
  other.grow_state_.store(kGrowStateIdle, std::memory_order_release);
  other.initial_capacity_ = 0;
  other.total_capacity_.store(0, std::memory_order_release);
  other.total_allocated_.store(0, std::memory_order_release);
  other.peak_usage_.store(0, std::memory_order_release);
  other.allocation_count_.store(0, std::memory_order_release);
  other.total_allocations_.store(0, std::memory_order_release);
  other.total_deallocations_.store(0, std::memory_order_release);
  other.alignment_waste_.store(0, std::memory_order_release);
  other.block_count_.store(0, std::memory_order_release);
}

inline StackAllocator::~StackAllocator() noexcept {
  FreeChain(head_.load(std::memory_order_acquire));
}

inline StackAllocator& StackAllocator::operator=(
    StackAllocator&& other) noexcept {
  if (this == &other) [[unlikely]] {
    return *this;
  }

  FreeChain(head_.load(std::memory_order_acquire));

  head_.store(other.head_.load(std::memory_order_acquire),
              std::memory_order_release);
  grow_state_.store(other.grow_state_.load(std::memory_order_acquire),
                    std::memory_order_release);
  initial_capacity_ = other.initial_capacity_;
  growth_ = other.growth_;
  total_capacity_.store(other.total_capacity_.load(std::memory_order_acquire),
                        std::memory_order_release);
  total_allocated_.store(other.total_allocated_.load(std::memory_order_acquire),
                         std::memory_order_release);
  peak_usage_.store(other.peak_usage_.load(std::memory_order_acquire),
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
  block_count_.store(other.block_count_.load(std::memory_order_acquire),
                     std::memory_order_release);

  other.head_.store(nullptr, std::memory_order_release);
  other.grow_state_.store(kGrowStateIdle, std::memory_order_release);
  other.initial_capacity_ = 0;
  other.total_capacity_.store(0, std::memory_order_release);
  other.total_allocated_.store(0, std::memory_order_release);
  other.peak_usage_.store(0, std::memory_order_release);
  other.allocation_count_.store(0, std::memory_order_release);
  other.total_allocations_.store(0, std::memory_order_release);
  other.total_deallocations_.store(0, std::memory_order_release);
  other.alignment_waste_.store(0, std::memory_order_release);
  other.block_count_.store(0, std::memory_order_release);

  return *this;
}

inline auto StackAllocator::GetMarker() const noexcept -> Marker {
  Block* const head = head_.load(std::memory_order_acquire);
  if (head == nullptr) {
    return {};
  }
  return {
      .block = head,
      .offset = head->offset.load(std::memory_order_acquire),
  };
}

inline void StackAllocator::RewindToMarker(Marker marker) noexcept {
  ROSCRAFT_ASSERT(marker.block != nullptr, "marker block cannot be null");
  auto* const target = static_cast<Block*>(marker.block);

  Block* current = head_.load(std::memory_order_acquire);
  while (current != nullptr && current != target) {
    Block* const next = current->next.load(std::memory_order_relaxed);
    const size_t capacity = current->capacity;
    std::destroy_at(current);
    AlignedFree(current);
    total_capacity_.fetch_sub(capacity, std::memory_order_relaxed);
    block_count_.fetch_sub(1, std::memory_order_relaxed);
    current = next;
  }

  ROSCRAFT_ASSERT(current == target, "marker block not found in stack chain");
  target->offset.store(marker.offset, std::memory_order_release);
  head_.store(target, std::memory_order_release);

  // Conservative state after bulk rewind.
  allocation_count_.store(0, std::memory_order_relaxed);
  total_allocated_.store(marker.offset, std::memory_order_relaxed);
}

inline void StackAllocator::Reset() noexcept {
  Block* const head = head_.load(std::memory_order_acquire);
  if (head == nullptr) {
    return;
  }

  FreeChain(head);
  Block* const fresh = CreateBlock(initial_capacity_);
  ROSCRAFT_VERIFY(fresh != nullptr, "failed to allocate block during reset");

  head_.store(fresh, std::memory_order_release);
  total_capacity_.store(initial_capacity_, std::memory_order_release);
  total_allocated_.store(0, std::memory_order_release);
  allocation_count_.store(0, std::memory_order_release);
  total_allocations_.store(0, std::memory_order_release);
  total_deallocations_.store(0, std::memory_order_release);
  alignment_waste_.store(0, std::memory_order_release);
  block_count_.store(1, std::memory_order_release);
}

inline AllocatorStats StackAllocator::Stats() const noexcept {
  return {
      .total_allocated = total_allocated_.load(std::memory_order_relaxed),
      .peak_usage = peak_usage_.load(std::memory_order_relaxed),
      .allocation_count = allocation_count_.load(std::memory_order_relaxed),
      .total_allocations = total_allocations_.load(std::memory_order_relaxed),
      .total_deallocations =
          total_deallocations_.load(std::memory_order_relaxed),
      .alignment_waste = alignment_waste_.load(std::memory_order_relaxed),
  };
}

inline auto StackAllocator::CreateBlock(size_t capacity) noexcept -> Block* {
  constexpr size_t kHeader = AlignUp(sizeof(Block), kDefaultAlignment);
  const size_t total_size = SaturatingAdd(kHeader, capacity);
  void* const raw = AlignedAlloc(kDefaultAlignment, total_size);
  if (raw == nullptr) [[unlikely]] {
    return nullptr;
  }

  auto* const block = std::construct_at(static_cast<Block*>(raw));
  block->buffer = static_cast<std::byte*>(raw) + kHeader;
  block->capacity = capacity;
  block->offset.store(0, std::memory_order_relaxed);
  block->next.store(nullptr, std::memory_order_relaxed);
  return block;
}

inline void StackAllocator::FreeChain(Block* head) noexcept {
  Block* current = head;
  while (current != nullptr) {
    Block* const next = current->next.load(std::memory_order_relaxed);
    std::destroy_at(current);
    AlignedFree(current);
    current = next;
  }
}

inline auto StackAllocator::TryReserve(Block& block, size_t size,
                                       size_t alignment) noexcept
    -> Reservation {
  constexpr size_t kHeaderSize = sizeof(AllocationHeader);
  const size_t header_alignment =
      std::max(alignment, alignof(AllocationHeader));

  size_t current_offset = block.offset.load(std::memory_order_acquire);
  for (;;) {
    auto* const start = static_cast<std::byte*>(block.buffer) + current_offset;
    const size_t padding =
        CalculatePaddingWithHeader(start, header_alignment, kHeaderSize);
    const size_t consumed = SaturatingAdd(padding, size);
    const size_t end_offset = SaturatingAdd(current_offset, consumed);

    if (end_offset > block.capacity) {
      return {};
    }

    if (block.offset.compare_exchange_weak(current_offset, end_offset,
                                           std::memory_order_release,
                                           std::memory_order_acquire)) {
      auto* const user_ptr = start + padding;
      auto* const header_ptr = std::launder(
          reinterpret_cast<AllocationHeader*>(user_ptr - kHeaderSize));
      header_ptr->previous_offset = current_offset;
      header_ptr->total_size = consumed;

      return {
          .ptr = user_ptr,
          .allocation_size = size,
          .consumed = consumed,
          .previous_offset = current_offset,
      };
    }
  }
}

inline auto StackAllocator::EnsureCapacity(size_t min_capacity) noexcept
    -> bool {
  Block* observed_head = head_.load(std::memory_order_acquire);
  const size_t current_capacity =
      observed_head != nullptr ? observed_head->capacity : initial_capacity_;
  const size_t desired_capacity =
      growth_.NextCapacity(current_capacity, min_capacity);
  if (desired_capacity < min_capacity || desired_capacity == 0) {
    return false;
  }

  uint8_t expected = kGrowStateIdle;
  if (!grow_state_.compare_exchange_strong(expected, kGrowStateGrowing,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
    grow_state_.wait(kGrowStateGrowing, std::memory_order_acquire);
    Block* const new_head = head_.load(std::memory_order_acquire);
    return new_head != nullptr && new_head->capacity >= min_capacity;
  }

  Block* const new_block = CreateBlock(desired_capacity);
  if (new_block == nullptr) {
    grow_state_.store(kGrowStateIdle, std::memory_order_release);
    grow_state_.notify_all();
    return false;
  }

  PublishBlock(new_block);
  grow_state_.store(kGrowStateIdle, std::memory_order_release);
  grow_state_.notify_all();
  return true;
}

inline void StackAllocator::PublishBlock(Block* block) noexcept {
  Block* expected_head = head_.load(std::memory_order_acquire);
  do {
    block->next.store(expected_head, std::memory_order_relaxed);
  } while (!head_.compare_exchange_weak(expected_head, block,
                                        std::memory_order_release,
                                        std::memory_order_acquire));

  total_capacity_.fetch_add(block->capacity, std::memory_order_relaxed);
  block_count_.fetch_add(1, std::memory_order_relaxed);
}

inline void StackAllocator::AccumulatePeak(std::atomic<size_t>& peak,
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

inline void* StackAllocator::do_allocate(size_t bytes, size_t alignment) {
  if (bytes == 0) [[unlikely]] {
    return nullptr;
  }

  if (!IsPowerOfTwo(alignment)) [[unlikely]] {
    PmrFailFast("alignment is not a power of two");
  }

  const size_t effective_alignment = std::max(alignment, kMinAlignment);
  Block* head = head_.load(std::memory_order_acquire);
  if (head != nullptr) {
    const Reservation reservation =
        TryReserve(*head, bytes, effective_alignment);
    if (reservation.ptr != nullptr) {
      allocation_count_.fetch_add(1, std::memory_order_relaxed);
      total_allocations_.fetch_add(1, std::memory_order_relaxed);
      total_allocated_.fetch_add(reservation.consumed,
                                 std::memory_order_relaxed);
      alignment_waste_.fetch_add(
          reservation.consumed - reservation.allocation_size,
          std::memory_order_relaxed);
      const size_t total = total_allocated_.load(std::memory_order_relaxed);
      AccumulatePeak(peak_usage_, total);
      return reservation.ptr;
    }
  }

  constexpr size_t kHeader = sizeof(AllocationHeader);
  const size_t min_capacity =
      SaturatingAdd(SaturatingAdd(bytes, effective_alignment), kHeader);
  if (!EnsureCapacity(min_capacity)) [[unlikely]] {
    PmrFailFast("unable to grow stack allocator block chain");
  }

  head = head_.load(std::memory_order_acquire);
  ROSCRAFT_ASSERT(head != nullptr, "head block must exist after grow");
  const Reservation reservation = TryReserve(*head, bytes, effective_alignment);
  if (reservation.ptr == nullptr) [[unlikely]] {
    PmrFailFast("reservation failed after successful stack grow");
  }

  allocation_count_.fetch_add(1, std::memory_order_relaxed);
  total_allocations_.fetch_add(1, std::memory_order_relaxed);
  total_allocated_.fetch_add(reservation.consumed, std::memory_order_relaxed);
  alignment_waste_.fetch_add(reservation.consumed - reservation.allocation_size,
                             std::memory_order_relaxed);
  const size_t total = total_allocated_.load(std::memory_order_relaxed);
  AccumulatePeak(peak_usage_, total);
  return reservation.ptr;
}

inline void StackAllocator::do_deallocate(void* p, size_t bytes,
                                          size_t /*alignment*/) {
  if (p == nullptr) [[unlikely]] {
    return;
  }

  Block* const head = head_.load(std::memory_order_acquire);
  if (head == nullptr) {
    return;
  }

  const auto addr = reinterpret_cast<uintptr_t>(p);
  const auto begin = reinterpret_cast<uintptr_t>(head->buffer);
  const auto end = begin + head->capacity;
  if (addr < begin || addr >= end) {
    // Not from head block: ignore by design for PMR compatibility.
    return;
  }

  auto* const header = std::launder(reinterpret_cast<AllocationHeader*>(
      static_cast<std::byte*>(p) - sizeof(AllocationHeader)));
  const size_t current_offset = head->offset.load(std::memory_order_acquire);

  const size_t expected_end =
      SaturatingAdd(header->previous_offset, header->total_size);
  if (expected_end != current_offset) {
    // Non-LIFO deallocation: ignore.
    return;
  }

  head->offset.store(header->previous_offset, std::memory_order_release);
  allocation_count_.fetch_sub(1, std::memory_order_relaxed);
  total_deallocations_.fetch_add(1, std::memory_order_relaxed);
  total_allocated_.fetch_sub(header->total_size, std::memory_order_relaxed);

  if (bytes > 0) {
    const size_t waste =
        header->total_size > bytes ? header->total_size - bytes : 0;
    alignment_waste_.fetch_sub(
        std::min(waste, alignment_waste_.load(std::memory_order_relaxed)),
        std::memory_order_relaxed);
  }
}

}  // namespace roscraft::memory
