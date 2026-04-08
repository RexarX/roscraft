#pragma once

#include <roscraft/container/static_string.hpp>

#include <cstdint>
#include <string_view>

namespace roscraft::bridge::network {

using IpAddress = container::StaticString<3 * 4 + 3>;

struct BridgeConfig {
  static constexpr std::string_view kDefaultHost = "127.0.0.1";
  static constexpr uint16_t kDefaultPort = 7401;

  IpAddress host{kDefaultHost};
  uint16_t port = kDefaultPort;

  bool allow_multiple_connections = false;
};

}  // namespace roscraft::bridge::network
