#pragma once

#include <roscraft/bridge/common/command/category.hpp>
#include <roscraft/bridge/common/command/common.hpp>
#include <roscraft/bridge/common/command/header.hpp>
#include <roscraft/bridge/common/command/payload.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

namespace roscraft::bridge::common {

/// @brief Complete command structure.
struct Command {
  CommandHeader header;
  CommandPayload payload;

  /// @brief Construct with kind and command id.
  /// @tparam Kind Command kind type (enum class)
  /// @param kind The command kind
  /// @param id Optional command identifier
  /// @return Command instance
  template <typename Kind>
  [[nodiscard]] static constexpr Command From(Kind kind, CommandId id = 0) {
    Command cmd;
    cmd.header = CommandHeader{
        .id = id,
        .category = GetCategory(kind),
        .kind_index = std::to_underlying(kind),
    };
    return cmd;
  }

  /// @brief Construct with kind, payload data and command id.
  /// @tparam Kind Command kind type (enum class)
  /// @param kind The command kind
  /// @param payload_data The payload data
  /// @param id Optional command identifier
  /// @return Command instance
  template <typename Kind>
  [[nodiscard]] static constexpr Command From(
      Kind kind, std::span<const std::byte> payload_data, CommandId id = 0) {
    Command cmd;
    cmd.header = CommandHeader{
        .id = id,
        .category = GetCategory(kind),
        .kind_index = std::to_underlying(kind),
    };
    cmd.payload.SetInline(payload_data);
    cmd.header.payload_size = cmd.payload.size;
    return cmd;
  }

  /// @brief Get mutable payload span.
  /// @return Mutable span of payload bytes
  [[nodiscard]] constexpr auto Payload() noexcept -> std::span<std::byte> {
    return payload.Data();
  }

  /// @brief Get read-only payload span.
  /// @return Read-only span of payload bytes
  [[nodiscard]] constexpr auto Payload() const noexcept
      -> std::span<const std::byte> {
    return payload.Data();
  }
};

static_assert(std::is_trivially_copyable_v<Command>,
              "Command should be trivially copyable");

}  // namespace roscraft::bridge::common
