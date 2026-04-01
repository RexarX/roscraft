#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>

namespace roscraft::bridge::common {

/// @brief Inline payload size (32 bytes covers most small responses).
inline constexpr size_t kResponseInlinePayloadSize = 32;

/// @brief Maximum payload size for arena allocation fallback.
inline constexpr size_t kResponseMaxPayloadSize = 4096;

/// @brief Response payload storage.
struct ResponsePayload {
  /// Inline payload storage
  alignas(alignof(std::max_align_t))
      std::array<std::byte, kResponseInlinePayloadSize> inline_data{};

  /// Pointer to arena-allocated data for large payloads
  std::byte* arena_data = nullptr;

  uint32_t size = 0;  ///< Total payload size

  /// @brief Check if payload uses inline storage.
  /// @return True if payload is stored inline, false if stored in arena
  [[nodiscard]] constexpr bool IsInline() const noexcept {
    return arena_data == nullptr && size <= kResponseInlinePayloadSize;
  }

  /// @brief Get payload as mutable span.
  /// @return Mutable span of the payload data
  [[nodiscard]] constexpr auto Data() noexcept -> std::span<std::byte> {
    if (size == 0) [[unlikely]] {
      return {};
    }

    if (IsInline()) {
      return {inline_data.data(), size};
    }
    return {arena_data, size};
  }

  /// @brief Get payload as read-only span.
  /// @return Read-only span of the payload data
  [[nodiscard]] constexpr auto Data() const noexcept
      -> std::span<const std::byte> {
    if (size == 0) [[unlikely]] {
      return {};
    }

    if (IsInline()) {
      return {inline_data.data(), size};
    }
    return {arena_data, size};
  }

  /// @brief Set inline payload from bytes.
  /// @param data Bytes to set as inline payload
  constexpr void SetInline(std::span<const std::byte> data) noexcept {
    const auto copy_size = std::min(data.size(), kResponseInlinePayloadSize);
    std::copy_n(data.begin(), copy_size, inline_data.begin());
    size = static_cast<uint32_t>(copy_size);
    arena_data = nullptr;
  }

  /// @brief Read payload as typed value.
  /// @tparam T Type to read as
  /// @return Optional span of the payload data as type T
  template <typename T>
  [[nodiscard]] constexpr auto As() const noexcept
      -> std::optional<std::span<const T>> {
    if (size < sizeof(T)) [[unlikely]] {
      return std::nullopt;
    }

    const auto* ptr = std::launder(reinterpret_cast<const T*>(
        IsInline() ? inline_data.data() : arena_data));
    return std::span<const T>(ptr, size / sizeof(T));
  }
};

static_assert(std::is_trivially_copyable_v<ResponsePayload>,
              "ResponsePayload should be trivially copyable");

}  // namespace roscraft::bridge::common
