#pragma once

#include <roscraft/utils/macro.hpp>
#include <roscraft/utils/type_info.hpp>

#include <concepts>
#include <cstdint>
#include <expected>
#include <string_view>
#include <type_traits>

namespace roscraft::bridge::common {

/// @brief Type index for commands.
using CommandTypeIndex = utils::TypeIndex;

/**
 * @brief Concept for valid command types.
 * @details A command must be destructible, default initializable,
 * move constructible and be an object.
 */
template <typename T>
concept CommandTrait =
    std::destructible<T> && std::default_initializable<T> &&
    std::move_constructible<T> && std::is_object_v<std::remove_cvref_t<T>>;

/**
 * @brief Retrieves the unqualified type name of a command type.
 * @tparam T Command type satisfying `CommandTrait`
 * @return Unqualified type name as string_view
 */
template <CommandTrait T>
[[nodiscard]] consteval std::string_view CommandNameOf() noexcept {
  return utils::TypeNameOf<T>();
}

/**
 * @brief Retrieves the unqualified type name of a command type.
 * @tparam T Command type satisfying `CommandTrait`
 * @param instance Command instance
 * @return Unqualified type name as string_view
 */
template <CommandTrait T>
[[nodiscard]] consteval std::string_view CommandNameOf(
    const T& /*instance*/) noexcept {
  return CommandNameOf<std::remove_cvref_t<T>>();
}

/// @brief Command identifier type.
using CommandId = uint64_t;

/// @brief Command priority levels (lower = higher priority).
enum class CommandPriority : uint8_t {
  kHigh = 0,
  kNormal = 1,
  kLow = 2,
  kBackground = 3,
};

/// @brief Command flags.
enum class CommandFlag : uint16_t {
  kNone = 0,
  kRequiresResponse = ROSCRAFT_BIT(0),
  kIsAsync = ROSCRAFT_BIT(1),
  kCanBeCancelled = ROSCRAFT_BIT(2),
  kHasTimeout = ROSCRAFT_BIT(3),
};

[[nodiscard]] constexpr CommandFlag operator|(CommandFlag a,
                                              CommandFlag b) noexcept {
  return static_cast<CommandFlag>(static_cast<uint16_t>(a) |
                                  static_cast<uint16_t>(b));
}

[[nodiscard]] constexpr CommandFlag operator&(CommandFlag a,
                                              CommandFlag b) noexcept {
  return static_cast<CommandFlag>(static_cast<uint16_t>(a) &
                                  static_cast<uint16_t>(b));
}

constexpr CommandFlag& operator|=(CommandFlag& a, CommandFlag b) noexcept {
  a = a | b;
  return a;
}

/// @brief Command error codes.
enum class CommandError : uint8_t {
  kNone = 0,
  kInvalidKind = 1,
  kInvalidPayload = 2,
  kQueueFull = 3,
  kBackendNotInitialized = 4,
  kBackendAlreadyInitialized = 5,
  kTimeout = 6,
  kInternalError = 7,
  kPermissionDenied = 8,
  kNotFound = 9,
  kSerializationError = 10,
  kDeserializationError = 11,
  kConnectionError = 12,
  kJniError = 13,
  kNetworkError = 14,
};

/// @brief Get human-readable name for `CommandError`.
/// @param error Error to convert
/// @return Human-readable error name
[[nodiscard]] constexpr std::string_view ToString(CommandError error) noexcept {
  switch (error) {
    case CommandError::kNone:
      return "None";
    case CommandError::kInvalidKind:
      return "InvalidKind";
    case CommandError::kInvalidPayload:
      return "InvalidPayload";
    case CommandError::kQueueFull:
      return "QueueFull";
    case CommandError::kBackendNotInitialized:
      return "BackendNotInitialized";
    case CommandError::kBackendAlreadyInitialized:
      return "BackendAlreadyInitialized";
    case CommandError::kTimeout:
      return "Timeout";
    case CommandError::kInternalError:
      return "InternalError";
    case CommandError::kPermissionDenied:
      return "PermissionDenied";
    case CommandError::kNotFound:
      return "NotFound";
    case CommandError::kSerializationError:
      return "SerializationError";
    case CommandError::kDeserializationError:
      return "DeserializationError";
    case CommandError::kConnectionError:
      return "ConnectionError";
    case CommandError::kJniError:
      return "JniError";
    case CommandError::kNetworkError:
      return "NetworkError";
  }
  return "Unknown";
}

/// @brief Result type for commands.
template <typename T>
using CommandResult = std::expected<T, CommandError>;

}  // namespace roscraft::bridge::common
