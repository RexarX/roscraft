#pragma once

#include <roscraft/container/static_string.hpp>

#include <cstdint>

namespace roscraft::bridge::network {

using IpAddress = container::StaticString<(3 * 4) + 3>;

struct BridgeConfig {
  static constexpr IpAddress kDefaultHost = "127.0.0.1";
  static constexpr uint16_t kDefaultPort = 7401;

  IpAddress host = kDefaultHost;
  uint16_t port = kDefaultPort;

  bool allow_multiple_connections = false;

  /// @brief Create a `BridgeConfig` from command line arguments.
  /// @param argc Argument count
  /// @param argv Argument values
  [[nodiscard]] static BridgeConfig From(int argc, char* argv[]);
};

}  // namespace roscraft::bridge::network
