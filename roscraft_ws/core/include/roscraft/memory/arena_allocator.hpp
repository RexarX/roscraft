#pragma once

#include <roscraft/assert.hpp>
#include <roscraft/memory/aligned_alloc.hpp>
#include <roscraft/memory/common.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory_resource>

namespace roscraft::memory {

/**
 * @brief Configuration for ArenaAllocator.
 * @details Controls initial capacity and growth behavior.
 */
struct ArenaOptions {
  size_t initial_capacity = 0;
  GrowthPolicy growth = GrowthPolicy::Fixed();
};

/**
 * @brief PMR arena allocator with lock-free hot allocation path.
 * @details Allocates memory from chained blocks using atomic bump pointers.
 *
 * Allocation hot path is lock-free and performs one CAS on the current head
 * block. When a block is exhausted, allocator enters a coordinated grow slow
 * path where a single thread allocates and publishes a new block.
 *
 * Individual deallocation is a no-op, matching monotonic arena semantics.
 * Memory is reclaimed by `Reset()` or destruction.
 *
 * @note Thread-safe for `allocate` and `deallocate`.
 * @warning `Reset()` must not run concurrently with allocation/deallocation.
 */
class ArenaAllocator final : public std::pmr::memory_resource {
public:
  /**
   * @brief Constructs arena allocator from options.
   * @warning Triggers assertion if `options.initial_capacity == 0`.
   * @param options Arena configuration
   */
  explicit ArenaAllocator(ArenaOptions options) noexcept;

  /**
   * @brief Constructs arena allocator with fixed capacity.
   * @warning Triggers assertion if `initial_capacity == 0`.
   * @param initial_capacity Capacity of initial block in bytes
   */
  explicit ArenaAllocator(size_t initial_capacity) noexcept;

  ArenaAllocator(const ArenaAllocator&) = delete;
  ArenaAllocator(ArenaAllocator&& other) noexcept;
  ~ArenaAllocator() noexcept override;

  ArenaAllocator& operator=(const ArenaAllocator&) = delete;
  ArenaAllocator& operator=(ArenaAllocator&& other) noexcept;

  /**
   * @brief Resets arena and reinitializes first block.
   * @details All existing allocations become invalid.
   * @warning Must not be called concurrently with `allocate`.
   */
  void Reset() noexcept;

  /**
   * @brief Returns true when no allocations are currently active.
   * @return True if arena is empty, false otherwise
   */
  [[nodiscard]] bool Empty() const noexcept {
    return allocation_count_.load(std::memory_order_relaxed) == 0;
  }

  /**
   * @brief Returns allocator statistics snapshot.
   * @return AllocatorStats for this arena
   */
  [[nodiscard]] AllocatorStats Stats() const noexcept;

  /**
   * @brief Returns total capacity across all active blocks.
   * @return Capacity in bytes
   */
  [[nodiscard]] size_t TotalCapacity() const noexcept {
    return total_capacity_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Returns initial block capacity.
   * @return Initial capacity in bytes
   */
  [[nodiscard]] size_t InitialCapacity() const noexcept {
    return initial_capacity_;
  }

  /**
   * @brief Returns current number of blocks.
   * @return Block count
   */
  [[nodiscard]] size_t BlockCount() const noexcept {
    return block_count_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Returns configured growth policy.
   * @return Growth policy
   */
  [[nodiscard]] GrowthPolicy Growth() const noexcept { return growth_; }

private:
  struct Block {
    void* buffer = nullptr;
    size_t capacity = 0;
    std::atomic<size_t> offset{0};
    std::atomic<Block*> next{nullptr};
  };

  struct Reservation {
    void* ptr = nullptr;
    size_t size = 0;
    size_t padding = 0;
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

  void do_deallocate(void* p, size_t bytes, size_t alignment) override {
    total_deallocations_.fetch_add(1, std::memory_order_relaxed);
  }

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

inline ArenaAllocator::ArenaAllocator(ArenaOptions options) noexcept
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

inline ArenaAllocator::ArenaAllocator(size_t initial_capacity) noexcept
    : ArenaAllocator(ArenaOptions{.initial_capacity = initial_capacity,
                                  .growth = GrowthPolicy::Fixed()}) {}

inline ArenaAllocator::ArenaAllocator(ArenaAllocator&& other) noexcept
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

inline ArenaAllocator::~ArenaAllocator() noexcept {
  FreeChain(head_.load(std::memory_order_acquire));
}

inline ArenaAllocator& ArenaAllocator::operator=(
    ArenaAllocator&& other) noexcept {
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

inline void ArenaAllocator::Reset() noexcept {
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

inline AllocatorStats ArenaAllocator::Stats() const noexcept {
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

inline auto ArenaAllocator::CreateBlock(size_t capacity) noexcept -> Block* {
  constexpr size_t kHeaderSize = AlignUp(sizeof(Block), kDefaultAlignment);
  const size_t total_size = SaturatingAdd(kHeaderSize, capacity);

  void* const raw = AlignedAlloc(kDefaultAlignment, total_size);
  if (raw == nullptr) [[unlikely]] {
    return nullptr;
  }

  auto* const block = std::construct_at(static_cast<Block*>(raw));
  block->buffer = static_cast<std::byte*>(raw) + kHeaderSize;
  block->capacity = capacity;
  block->offset.store(0, std::memory_order_relaxed);
  block->next.store(nullptr, std::memory_order_relaxed);
  return block;
}

inline void ArenaAllocator::FreeChain(Block* head) noexcept {
  Block* current = head;
  while (current != nullptr) {
    Block* const next = current->next.load(std::memory_order_relaxed);
    std::destroy_at(current);
    AlignedFree(current);
    current = next;
  }
}

inline auto ArenaAllocator::TryReserve(Block& block, size_t size,
                                       size_t alignment) noexcept
    -> Reservation {
  size_t current_offset = block.offset.load(std::memory_order_acquire);
  for (;;) {
    auto* const begin = static_cast<std::byte*>(block.buffer) + current_offset;
    const size_t padding = CalculatePadding(begin, alignment);
    const size_t aligned_offset = SaturatingAdd(current_offset, padding);
    const size_t end_offset = SaturatingAdd(aligned_offset, size);

    if (end_offset > block.capacity) {
      return {};
    }

    if (block.offset.compare_exchange_weak(current_offset, end_offset,
                                           std::memory_order_release,
                                           std::memory_order_acquire)) {
      return {
          .ptr = static_cast<std::byte*>(block.buffer) + aligned_offset,
          .size = size,
          .padding = padding,
      };
    }
  }
}

inline bool ArenaAllocator::EnsureCapacity(size_t min_capacity) noexcept {
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

  Block* new_block = CreateBlock(desired_capacity);
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

inline void ArenaAllocator::PublishBlock(Block* block) noexcept {
  Block* expected_head = head_.load(std::memory_order_acquire);
  do {
    block->next.store(expected_head, std::memory_order_relaxed);
  } while (!head_.compare_exchange_weak(expected_head, block,
                                        std::memory_order_release,
                                        std::memory_order_acquire));

  total_capacity_.fetch_add(block->capacity, std::memory_order_relaxed);
  block_count_.fetch_add(1, std::memory_order_relaxed);
}

inline void ArenaAllocator::AccumulatePeak(std::atomic<size_t>& peak,
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

inline void* ArenaAllocator::do_allocate(size_t bytes, size_t alignment) {
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
      alignment_waste_.fetch_add(reservation.padding,
                                 std::memory_order_relaxed);
      const size_t total =
          total_allocated_.fetch_add(bytes, std::memory_order_relaxed) + bytes;
      AccumulatePeak(peak_usage_, total);
      return reservation.ptr;
    }
  }

  const size_t min_capacity = SaturatingAdd(bytes, effective_alignment);
  if (!EnsureCapacity(min_capacity)) [[unlikely]] {
    PmrFailFast("unable to grow arena block chain");
  }

  head = head_.load(std::memory_order_acquire);
  ROSCRAFT_ASSERT(head != nullptr, "head block must exist after grow");
  const Reservation reservation = TryReserve(*head, bytes, effective_alignment);
  if (reservation.ptr == nullptr) [[unlikely]] {
    PmrFailFast("reservation failed after successful growth");
  }

  allocation_count_.fetch_add(1, std::memory_order_relaxed);
  total_allocations_.fetch_add(1, std::memory_order_relaxed);
  alignment_waste_.fetch_add(reservation.padding, std::memory_order_relaxed);
  const size_t total =
      total_allocated_.fetch_add(bytes, std::memory_order_relaxed) + bytes;
  AccumulatePeak(peak_usage_, total);
  return reservation.ptr;
}

}  // namespace roscraft::memory
