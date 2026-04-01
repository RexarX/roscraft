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

/// @brief Inline payload size (32 bytes covers most small commands).
inline constexpr size_t kCommandInlinePayloadSize = 32;

/// @brief Maximum payload size for arena allocation fallback.
inline constexpr size_t kCommandMaxPayloadSize = 4096;

/// @brief Command payload storage.
/// @details Small payloads use inline storage, large payloads use arena.
struct CommandPayload {
  /// Inline payload storage
  alignas(alignof(std::max_align_t))
      std::array<std::byte, kCommandInlinePayloadSize> inline_data{};

  /// Pointer to arena-allocated data for large payloads
  std::byte* arena_data = nullptr;
  uint32_t size = 0;  ///< Total payload size

  /// @brief Check if payload uses inline storage.
  /// @return True if payload is stored inline
  [[nodiscard]] constexpr bool IsInline() const noexcept {
    return arena_data == nullptr && size <= kCommandInlinePayloadSize;
  }

  /// @brief Get payload as mutable span.
  /// @return Mutable span of payload bytes
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
  /// @return Read-only span of payload bytes
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
    const auto copy_size = std::min(data.size(), kCommandInlinePayloadSize);
    std::copy_n(data.begin(), copy_size, inline_data.begin());
    size = static_cast<uint32_t>(copy_size);
    arena_data = nullptr;
  }

  /// @brief Read payload as typed value.
  /// @return Optional span of typed values, or nullopt if too small.
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

static_assert(std::is_trivially_copyable_v<CommandPayload>,
              "CommandPayload should be trivially copyable");

}  // namespace roscraft::bridge::common
