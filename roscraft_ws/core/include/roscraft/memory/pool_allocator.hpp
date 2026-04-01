#pragma once

#include <roscraft/assert.hpp>
#include <roscraft/memory/aligned_alloc.hpp>
#include <roscraft/memory/common.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory_resource>

namespace roscraft::memory {

/// @brief Configuration for PoolAllocator.
struct PoolAllocatorOptions {
  size_t block_size = 0;
  size_t block_count = 0;
  size_t alignment = kDefaultAlignment;
  GrowthPolicy growth = GrowthPolicy::Fixed();
};

/**
 * @brief Lock-free PMR pool allocator for fixed-size blocks.
 * @details Allocates from fixed-size blocks stored in growable chunks.
 *
 * Fast path uses lock-free freelist push/pop based on embedded next pointers.
 * Growth allocates and links a new chunk, then pushes all its blocks into the
 * freelist.
 */
class PoolAllocator final : public std::pmr::memory_resource {
public:
  /**
   * @brief Constructs pool allocator from options.
   * @warning Triggers assertion when block_size or block_count is zero.
   * @param options Pool allocator options
   */
  explicit PoolAllocator(PoolAllocatorOptions options) noexcept;

  /**
   * @brief Constructs pool allocator with fixed growth.
   * @param block_size Size of each block in bytes
   * @param block_count Number of blocks in initial chunk
   * @param alignment Block alignment
   */
  PoolAllocator(size_t block_size, size_t block_count,
                size_t alignment = kDefaultAlignment) noexcept;

  template <typename T>
  [[nodiscard]] static auto ForType(size_t block_count) noexcept
      -> PoolAllocator;

  PoolAllocator(const PoolAllocator&) = delete;
  PoolAllocator(PoolAllocator&& other) noexcept;
  ~PoolAllocator() noexcept override;

  PoolAllocator& operator=(const PoolAllocator&) = delete;
  PoolAllocator& operator=(PoolAllocator&& other) noexcept;

  /**
   * @brief Resets pool to fully free state.
   * @warning Must not be called concurrently with allocate/deallocate.
   */
  void Reset() noexcept;

  /**
   * @brief Returns true when no free blocks remain.
   * @return True if full, false otherwise
   */
  [[nodiscard]] bool Full() const noexcept;

  /**
   * @brief Returns true when all blocks are free.
   * @return True if empty, false otherwise
   */
  [[nodiscard]] bool Empty() const noexcept;

  /**
   * @brief Checks pointer ownership.
   * @param ptr Pointer to test
   * @return True when pointer belongs to any pool chunk, false otherwise
   */
  [[nodiscard]] bool Owns(const void* ptr) const noexcept;

  /**
   * @brief Returns runtime stats.
   * @return Allocator stats
   */
  [[nodiscard]] AllocatorStats Stats() const noexcept;

  /**
   * @brief Returns effective block size.
   * @return Block size in bytes
   */
  [[nodiscard]] size_t BlockSize() const noexcept { return block_size_; }

  /**
   * @brief Returns configured block alignment.
   * @return Alignment in bytes
   */
  [[nodiscard]] size_t Alignment() const noexcept { return alignment_; }

  /**
   * @brief Returns number of blocks in first chunk.
   * @return Initial block count
   */
  [[nodiscard]] size_t InitialBlockCount() const noexcept {
    return initial_block_count_;
  }

  /**
   * @brief Returns total blocks across chunks.
   * @return Total block count
   */
  [[nodiscard]] size_t BlockCount() const noexcept {
    return total_blocks_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Returns free block count.
   * @return Free block count
   */
  [[nodiscard]] size_t FreeBlockCount() const noexcept {
    return free_blocks_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Returns used block count.
   * @return Used block count
   */
  [[nodiscard]] size_t UsedBlockCount() const noexcept;

  /**
   * @brief Returns growth policy.
   * @return Growth policy
   */
  [[nodiscard]] GrowthPolicy Growth() const noexcept { return growth_; }

private:
  struct ChunkHeader {
    void* buffer = nullptr;
    size_t capacity = 0;
    size_t block_count = 0;
    std::atomic<ChunkHeader*> next{nullptr};
  };

  static constexpr uint8_t kGrowStateIdle = 0;
  static constexpr uint8_t kGrowStateGrowing = 1;

  [[nodiscard]] static ChunkHeader* CreateChunk(size_t block_size,
                                                size_t block_count,
                                                size_t alignment) noexcept;
  static void FreeChunkChain(ChunkHeader* chunk) noexcept;
  static void PushBlock(std::atomic<void*>& head, void* block) noexcept;
  [[nodiscard]] static void* PopBlock(std::atomic<void*>& head) noexcept;
  static void AccumulatePeak(std::atomic<size_t>& peak,
                             size_t candidate) noexcept;

  [[nodiscard]] bool GrowIfNeeded() noexcept;
  void PushChunkBlocks(ChunkHeader& chunk) noexcept;
  void RebuildFreeList() noexcept;

  [[nodiscard]] void* do_allocate(size_t bytes, size_t alignment) override;
  void do_deallocate(void* p, size_t bytes, size_t alignment) override;
  [[nodiscard]] bool do_is_equal(
      const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }

  size_t block_size_ = 0;
  size_t initial_block_count_ = 0;
  size_t alignment_ = 0;
  GrowthPolicy growth_{};

  std::atomic<void*> free_head_{nullptr};
  std::atomic<ChunkHeader*> chunks_{nullptr};
  std::atomic<uint8_t> grow_state_{kGrowStateIdle};

  std::atomic<size_t> total_blocks_{0};
  std::atomic<size_t> free_blocks_{0};
  std::atomic<size_t> peak_used_blocks_{0};
  std::atomic<size_t> total_allocations_{0};
  std::atomic<size_t> total_deallocations_{0};
};

inline PoolAllocator::PoolAllocator(PoolAllocatorOptions options) noexcept
    : block_size_(std::max(options.block_size, sizeof(void*))),
      initial_block_count_(options.block_count),
      alignment_(options.alignment),
      growth_(options.growth) {
  ROSCRAFT_ASSERT(block_size_ > 0, "block_size must be greater than zero");
  ROSCRAFT_ASSERT(initial_block_count_ > 0,
                  "block_count must be greater than zero");
  ROSCRAFT_ASSERT(IsPowerOfTwo(alignment_),
                  "alignment '{}' must be power of two", alignment_);
  ROSCRAFT_ASSERT(alignment_ >= alignof(void*),
                  "alignment '{}' must be >= '{}'", alignment_, alignof(void*));

  block_size_ = AlignUp(block_size_, alignment_);

  ChunkHeader* const initial_chunk =
      CreateChunk(block_size_, initial_block_count_, alignment_);
  ROSCRAFT_VERIFY(initial_chunk != nullptr, "failed to allocate pool chunk");

  chunks_.store(initial_chunk, std::memory_order_release);
  total_blocks_.store(initial_chunk->block_count, std::memory_order_relaxed);
  free_blocks_.store(initial_chunk->block_count, std::memory_order_relaxed);
  PushChunkBlocks(*initial_chunk);
}

inline PoolAllocator::PoolAllocator(size_t block_size, size_t block_count,
                                    size_t alignment) noexcept
    : PoolAllocator(PoolAllocatorOptions{.block_size = block_size,
                                         .block_count = block_count,
                                         .alignment = alignment,
                                         .growth = GrowthPolicy::Fixed()}) {}

template <typename T>
inline PoolAllocator PoolAllocator::ForType(size_t block_count) noexcept {
  constexpr size_t kAlign =
      alignof(T) > alignof(void*) ? alignof(T) : alignof(void*);
  return {sizeof(T), block_count, kAlign};
}

inline PoolAllocator::PoolAllocator(PoolAllocator&& other) noexcept
    : block_size_(other.block_size_),
      initial_block_count_(other.initial_block_count_),
      alignment_(other.alignment_),
      growth_(other.growth_),
      free_head_(other.free_head_.load(std::memory_order_acquire)),
      chunks_(other.chunks_.load(std::memory_order_acquire)),
      grow_state_(other.grow_state_.load(std::memory_order_acquire)),
      total_blocks_(other.total_blocks_.load(std::memory_order_acquire)),
      free_blocks_(other.free_blocks_.load(std::memory_order_acquire)),
      peak_used_blocks_(
          other.peak_used_blocks_.load(std::memory_order_acquire)),
      total_allocations_(
          other.total_allocations_.load(std::memory_order_acquire)),
      total_deallocations_(
          other.total_deallocations_.load(std::memory_order_acquire)) {
  other.block_size_ = 0;
  other.initial_block_count_ = 0;
  other.alignment_ = 0;
  other.free_head_.store(nullptr, std::memory_order_release);
  other.chunks_.store(nullptr, std::memory_order_release);
  other.grow_state_.store(kGrowStateIdle, std::memory_order_release);
  other.total_blocks_.store(0, std::memory_order_release);
  other.free_blocks_.store(0, std::memory_order_release);
  other.peak_used_blocks_.store(0, std::memory_order_release);
  other.total_allocations_.store(0, std::memory_order_release);
  other.total_deallocations_.store(0, std::memory_order_release);
}

inline PoolAllocator::~PoolAllocator() noexcept {
  FreeChunkChain(chunks_.load(std::memory_order_acquire));
}

inline PoolAllocator& PoolAllocator::operator=(PoolAllocator&& other) noexcept {
  if (this == &other) [[unlikely]] {
    return *this;
  }

  FreeChunkChain(chunks_.load(std::memory_order_acquire));

  block_size_ = other.block_size_;
  initial_block_count_ = other.initial_block_count_;
  alignment_ = other.alignment_;
  growth_ = other.growth_;
  free_head_.store(other.free_head_.load(std::memory_order_acquire),
                   std::memory_order_release);
  chunks_.store(other.chunks_.load(std::memory_order_acquire),
                std::memory_order_release);
  grow_state_.store(other.grow_state_.load(std::memory_order_acquire),
                    std::memory_order_release);
  total_blocks_.store(other.total_blocks_.load(std::memory_order_acquire),
                      std::memory_order_release);
  free_blocks_.store(other.free_blocks_.load(std::memory_order_acquire),
                     std::memory_order_release);
  peak_used_blocks_.store(
      other.peak_used_blocks_.load(std::memory_order_acquire),
      std::memory_order_release);
  total_allocations_.store(
      other.total_allocations_.load(std::memory_order_acquire),
      std::memory_order_release);
  total_deallocations_.store(
      other.total_deallocations_.load(std::memory_order_acquire),
      std::memory_order_release);

  other.block_size_ = 0;
  other.initial_block_count_ = 0;
  other.alignment_ = 0;
  other.free_head_.store(nullptr, std::memory_order_release);
  other.chunks_.store(nullptr, std::memory_order_release);
  other.grow_state_.store(kGrowStateIdle, std::memory_order_release);
  other.total_blocks_.store(0, std::memory_order_release);
  other.free_blocks_.store(0, std::memory_order_release);
  other.peak_used_blocks_.store(0, std::memory_order_release);
  other.total_allocations_.store(0, std::memory_order_release);
  other.total_deallocations_.store(0, std::memory_order_release);

  return *this;
}

inline void PoolAllocator::Reset() noexcept {
  RebuildFreeList();
  total_allocations_.store(0, std::memory_order_release);
  total_deallocations_.store(0, std::memory_order_release);
}

inline auto PoolAllocator::Full() const noexcept -> bool {
  return free_blocks_.load(std::memory_order_relaxed) == 0;
}

inline auto PoolAllocator::Empty() const noexcept -> bool {
  return free_blocks_.load(std::memory_order_relaxed) ==
         total_blocks_.load(std::memory_order_relaxed);
}

inline auto PoolAllocator::Owns(const void* ptr) const noexcept -> bool {
  if (ptr == nullptr) [[unlikely]] {
    return false;
  }

  const auto addr = reinterpret_cast<uintptr_t>(ptr);
  ChunkHeader* chunk = chunks_.load(std::memory_order_acquire);
  while (chunk != nullptr) {
    const auto begin = reinterpret_cast<uintptr_t>(chunk->buffer);
    const auto end = begin + chunk->capacity;
    if (addr >= begin && addr < end) {
      return ((addr - begin) % block_size_) == 0;
    }
    chunk = chunk->next.load(std::memory_order_acquire);
  }

  return false;
}

inline auto PoolAllocator::Stats() const noexcept -> AllocatorStats {
  const size_t total = total_blocks_.load(std::memory_order_relaxed);
  const size_t free = free_blocks_.load(std::memory_order_relaxed);
  const size_t used = total >= free ? total - free : 0;
  const size_t peak = peak_used_blocks_.load(std::memory_order_relaxed);

  return {
      .total_allocated = SaturatingMul(used, block_size_),
      .peak_usage = SaturatingMul(peak, block_size_),
      .allocation_count = used,
      .total_allocations = total_allocations_.load(std::memory_order_relaxed),
      .total_deallocations =
          total_deallocations_.load(std::memory_order_relaxed),
      .alignment_waste = 0,
  };
}

inline size_t PoolAllocator::UsedBlockCount() const noexcept {
  const size_t total = total_blocks_.load(std::memory_order_relaxed);
  const size_t free = free_blocks_.load(std::memory_order_relaxed);
  return total >= free ? total - free : 0;
}

inline auto PoolAllocator::CreateChunk(size_t block_size, size_t block_count,
                                       size_t alignment) noexcept
    -> ChunkHeader* {
  constexpr size_t kHeaderAlignment = alignof(ChunkHeader);
  const size_t chunk_alignment =
      std::max({alignment, kHeaderAlignment, kMinAlignment});
  const size_t header_size = AlignUp(sizeof(ChunkHeader), chunk_alignment);
  const size_t payload = SaturatingMul(block_size, block_count);
  const size_t total_size = SaturatingAdd(header_size, payload);

  void* const raw = AlignedAlloc(chunk_alignment, total_size);
  if (raw == nullptr) [[unlikely]] {
    return nullptr;
  }

  auto* const chunk = std::construct_at(static_cast<ChunkHeader*>(raw));
  chunk->buffer = static_cast<std::byte*>(raw) + header_size;
  chunk->capacity = payload;
  chunk->block_count = block_count;
  chunk->next.store(nullptr, std::memory_order_relaxed);
  return chunk;
}

inline void PoolAllocator::FreeChunkChain(ChunkHeader* chunk) noexcept {
  ChunkHeader* current = chunk;
  while (current != nullptr) {
    ChunkHeader* const next = current->next.load(std::memory_order_relaxed);
    std::destroy_at(current);
    AlignedFree(current);
    current = next;
  }
}

inline void PoolAllocator::PushBlock(std::atomic<void*>& head,
                                     void* block) noexcept {
  void* observed = head.load(std::memory_order_acquire);
  for (;;) {
    *static_cast<void**>(block) = observed;
    if (head.compare_exchange_weak(observed, block, std::memory_order_release,
                                   std::memory_order_acquire)) {
      return;
    }
  }
}

inline void* PoolAllocator::PopBlock(std::atomic<void*>& head) noexcept {
  void* observed = head.load(std::memory_order_acquire);
  for (;;) {
    if (observed == nullptr) {
      return nullptr;
    }
    void* const next = *static_cast<void**>(observed);
    if (head.compare_exchange_weak(observed, next, std::memory_order_release,
                                   std::memory_order_acquire)) {
      return observed;
    }
  }
}

inline void PoolAllocator::AccumulatePeak(std::atomic<size_t>& peak,
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

inline bool PoolAllocator::GrowIfNeeded() noexcept {
  const size_t current_blocks = total_blocks_.load(std::memory_order_acquire);
  const size_t desired_blocks =
      growth_.NextCapacity(current_blocks, current_blocks + 1);
  if (desired_blocks <= current_blocks) {
    return false;
  }

  uint8_t expected = kGrowStateIdle;
  if (!grow_state_.compare_exchange_strong(expected, kGrowStateGrowing,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
    grow_state_.wait(kGrowStateGrowing, std::memory_order_acquire);
    return free_head_.load(std::memory_order_acquire) != nullptr;
  }

  const size_t chunk_blocks = desired_blocks - current_blocks;
  ChunkHeader* const chunk = CreateChunk(block_size_, chunk_blocks, alignment_);
  if (chunk == nullptr) {
    grow_state_.store(kGrowStateIdle, std::memory_order_release);
    grow_state_.notify_all();
    return false;
  }

  ChunkHeader* observed = chunks_.load(std::memory_order_acquire);
  do {
    chunk->next.store(observed, std::memory_order_relaxed);
  } while (!chunks_.compare_exchange_weak(
      observed, chunk, std::memory_order_release, std::memory_order_acquire));

  total_blocks_.fetch_add(chunk->block_count, std::memory_order_relaxed);
  free_blocks_.fetch_add(chunk->block_count, std::memory_order_relaxed);
  PushChunkBlocks(*chunk);

  grow_state_.store(kGrowStateIdle, std::memory_order_release);
  grow_state_.notify_all();
  return true;
}

inline void PoolAllocator::PushChunkBlocks(ChunkHeader& chunk) noexcept {
  auto* current = static_cast<std::byte*>(chunk.buffer);
  for (size_t i = 0; i < chunk.block_count; ++i) {
    PushBlock(free_head_, current);
    current += block_size_;
  }
}

inline void PoolAllocator::RebuildFreeList() noexcept {
  free_head_.store(nullptr, std::memory_order_release);
  ChunkHeader* chunk = chunks_.load(std::memory_order_acquire);
  size_t total = 0;

  while (chunk != nullptr) {
    PushChunkBlocks(*chunk);
    total += chunk->block_count;
    chunk = chunk->next.load(std::memory_order_acquire);
  }

  total_blocks_.store(total, std::memory_order_release);
  free_blocks_.store(total, std::memory_order_release);
}

inline void* PoolAllocator::do_allocate(size_t bytes, size_t alignment) {
  if (bytes == 0) [[unlikely]] {
    return nullptr;
  }

  if (!IsPowerOfTwo(alignment)) [[unlikely]] {
    PmrFailFast("alignment is not a power of two");
  }

  if (alignment > alignment_) [[unlikely]] {
    PmrFailFast("requested alignment exceeds pool alignment");
  }

  if (bytes > block_size_) [[unlikely]] {
    PmrFailFast("requested size exceeds pool block size");
  }

  void* block = PopBlock(free_head_);
  if (block == nullptr) {
    if (!GrowIfNeeded()) [[unlikely]] {
      PmrFailFast("pool exhausted and growth failed");
    }

    block = PopBlock(free_head_);
    if (block == nullptr) [[unlikely]] {
      PmrFailFast("pool exhausted after growth");
    }
  }

  const size_t free_now =
      free_blocks_.fetch_sub(1, std::memory_order_relaxed) - 1;
  total_allocations_.fetch_add(1, std::memory_order_relaxed);

  const size_t total = total_blocks_.load(std::memory_order_relaxed);
  const size_t used = total - free_now;
  AccumulatePeak(peak_used_blocks_, used);
  return block;
}

inline void PoolAllocator::do_deallocate(void* p, size_t /*bytes*/,
                                         size_t /*alignment*/) {
  if (p == nullptr) [[unlikely]] {
    return;
  }

  if (!Owns(p)) [[unlikely]] {
    ROSCRAFT_ASSERT(false, "pointer does not belong to pool allocator");
    return;
  }

  PushBlock(free_head_, p);
  free_blocks_.fetch_add(1, std::memory_order_relaxed);
  total_deallocations_.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace roscraft::memory
