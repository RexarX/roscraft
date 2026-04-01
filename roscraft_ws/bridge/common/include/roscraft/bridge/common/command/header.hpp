#pragma once

#include <roscraft/bridge/common/command/common.hpp>

#include <cstddef>
#include <cstdint>
#include <source_location>
#include <type_traits>

namespace roscraft::bridge::common {

/// @brief Command header (fixed-size, cache-friendly).
/// @details 24 bytes, fits in single cache line with padding.
struct CommandHeader {
  CommandId id = 0;  ///< Unique command identifier

  /// Command category for routing
  CommandCategory category = CommandCategory::kCore;
  uint8_t kind_index = 0;  ///< Category-specific kind index
  CommandPriority priority = CommandPriority::kNormal;  ///< Command priority

  /// Additional flags
  uint16_t flags = static_cast<uint16_t>(CommandFlag::kRequiresResponse);
  uint32_t payload_size = 0;  ///< Payload size in bytes

  /// @brief Get kind as `CoreCommand`.
  /// @return CoreCommand kind
  [[nodiscard]] constexpr CoreCommand AsCoreCommand() const noexcept {
    return static_cast<CoreCommand>(kind_index);
  }

  /// @brief Get kind as `MinecraftCommand`.
  /// @return MinecraftCommand kind
  [[nodiscard]] constexpr MinecraftCommand AsMinecraftCommand() const noexcept {
    return static_cast<MinecraftCommand>(kind_index);
  }

  /// @brief Get kind as `Ros2Command`.
  /// @return Ros2Command kind
  [[nodiscard]] constexpr Ros2Command AsRos2Command() const noexcept {
    return static_cast<Ros2Command>(kind_index);
  }

  /// @brief Get kind as `RobotCommand`.
  /// @return RobotCommand kind
  [[nodiscard]] constexpr RobotCommand AsRobotCommand() const noexcept {
    return static_cast<RobotCommand>(kind_index);
  }

  /// @brief Check if flag is set.
  /// @param flag Flag to check
  /// @return True if flag is set
  [[nodiscard]] constexpr bool HasFlag(CommandFlag flag) const noexcept {
    return (flags & static_cast<uint16_t>(flag)) != 0;
  }
};

static_assert(sizeof(CommandHeader) <= 64,
              "CommandHeader should fit in cache line");
static_assert(alignof(CommandHeader) <= alignof(std::max_align_t),
              "CommandHeader alignment should be standard");
static_assert(std::is_trivially_copyable_v<CommandHeader>,
              "CommandHeader should be trivially copyable");

}  // namespace roscraft::bridge::common
