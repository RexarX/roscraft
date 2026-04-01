#pragma once

#include <roscraft/assert.hpp>
#include <roscraft/memory/common.hpp>

#include <cstddef>
#include <cstdlib>

namespace roscraft::memory {

/**
 * @brief Allocates memory with the specified alignment.
 * @details Uses `_aligned_malloc` on MSVC and `std::aligned_alloc` on other
 * platforms.
 * @warning Triggers assertion in next cases:
 * - alignment is zero.
 * - alignment is not a power of two.
 * - size is zero.
 *
 * @param alignment The alignment in bytes (must be a power of two and non-zero)
 * @param size The size of the memory block to allocate in bytes (must be
 * non-zero)
 * @return A pointer to the allocated memory block, or `nullptr` on failure
 */
[[nodiscard]] inline void* AlignedAlloc(size_t alignment,
                                        size_t size) noexcept {
  ROSCRAFT_ASSERT(alignment != 0, "alignment cannot be zero!");

  const auto is_power_of_two = [](size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
  };
  ROSCRAFT_ASSERT(is_power_of_two(alignment),
                  "alignment must be a power of two!");
  ROSCRAFT_ASSERT(size != 0, "size cannot be zero!");

#ifdef _MSC_VER
  return _aligned_malloc(size, alignment);
#else
  // POSIX aligned_alloc requires size to be a multiple of alignment.
  return std::aligned_alloc(alignment, AlignUp(size, alignment));
#endif
}

/**
 * @brief Frees memory allocated with `AlignedAlloc`.
 * @details Uses `_aligned_free` on MSVC and `std::free` on other platforms.
 * @warning Triggers assertion if ptr is `nullptr`.
 * @param ptr The pointer to the memory block to free
 */
inline void AlignedFree(void* ptr) noexcept {
  if (ptr == nullptr) [[unlikely]] {
    return;
  }

#ifdef _MSC_VER
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

}  // namespace roscraft::memory
