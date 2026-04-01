#pragma once

#include <cstdint>
#include <string_view>

namespace roscraft::bridge::common {

/// @brief Command category for routing.
enum class CommandCategory : uint8_t {
  kCore = 0,
  kMinecraft = 1,
  kRos2 = 2,
  kRobot = 3,
};

/// @brief Core command kinds (backend management).
enum class CoreCommand : uint8_t {
  kInitialize = 0,
  kShutdown = 1,
  kGetStatus = 2,
  kSwitchBackend = 3,
  kGetCapabilities = 4,
};

/// @brief Minecraft command kinds.
enum class MinecraftCommand : uint8_t {
  kBlockPlace = 0,
  kBlockRemove = 1,
  kEntityQuery = 2,
  kWorldQuery = 3,
  kPlayerQuery = 4,
  kChunkLoad = 5,
  kChunkUnload = 6,
};

/// @brief ROS2 command kinds.
enum class Ros2Command : uint8_t {
  kTopicPublish = 0,
  kTopicSubscribe = 1,
  kTopicUnsubscribe = 2,
  kServiceCall = 3,
  kServiceAdvertise = 4,
  kServiceUnadvertise = 5,
  kActionSendGoal = 6,
  kActionCancel = 7,
  kActionGetResult = 8,
};

/// @brief Robot command kinds.
enum class RobotCommand : uint8_t {
  kSpawn = 0,
  kRemove = 1,
  kUpdateTransform = 2,
  kGetState = 3,
  kSetJointState = 4,
  kGetJointState = 5,
};

/// @brief Get command category from core command.
/// @param command Core command to get category for
/// @return Category of the command
[[nodiscard]] consteval CommandCategory GetCategory(
    CoreCommand /*command*/) noexcept {
  return CommandCategory::kCore;
}

/// @brief Get command category from Minecraft command.
/// @param command Core command to get category for
/// @return Category of the command
[[nodiscard]] consteval CommandCategory GetCategory(
    MinecraftCommand /*command*/) noexcept {
  return CommandCategory::kMinecraft;
}

/// @brief Get command category from ROS2 command.
/// @param command Core command to get category for
/// @return Category of the command
[[nodiscard]] consteval CommandCategory GetCategory(
    Ros2Command /*command*/) noexcept {
  return CommandCategory::kRos2;
}

/// @brief Get command category from Robot command.
/// @param command Core command to get category for
/// @return Category of the command
[[nodiscard]] consteval CommandCategory GetCategory(
    RobotCommand /*command*/) noexcept {
  return CommandCategory::kRobot;
}

/// @brief Get human-readable name for `CommandCategory`.
/// @param category Category to convert
/// @return Human-readable category name
[[nodiscard]] constexpr std::string_view ToString(
    CommandCategory category) noexcept {
  switch (category) {
    case CommandCategory::kCore:
      return "Core";
    case CommandCategory::kMinecraft:
      return "Minecraft";
    case CommandCategory::kRos2:
      return "Ros2";
    case CommandCategory::kRobot:
      return "Robot";
  }
  return "Unknown";
}

}  // namespace roscraft::bridge::common
