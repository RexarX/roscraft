#pragma once

#include <roscraft/bridge/common/backend.hpp>

#include <cstdint>
#include <string_view>

namespace roscraft::bridge::common {

/// @brief Network bridge configuration.
struct NetworkConfig {
  /// Network host for network backend
  std::string_view host = "127.0.0.1";
  uint16_t port = 7401;  ///< Network port for network backend
};

/// @brief Bridge configuration.
struct BridgeConfig {
  BackendType backend_type = BackendType::kNone;  ///< Backend type to use
  uint32_t command_timeout_ms = 5000;  ///< Command timeout in milliseconds

  /// Platform-specific data (e.g., JavaVM pointer for JNI)
  void* platform_data = nullptr;
  NetworkConfig network;
};

}  // namespace roscraft::bridge::common
