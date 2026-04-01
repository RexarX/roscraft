#pragma once

#include <cstdint>

namespace roscraft::bridge::common {

enum class BackendType : uint8_t {
  kJni,
  kNetwork,
  kNone,
};

/// @brief Backend status.
enum class BackendStatus : uint8_t {
  kUninitialized = 0,
  kInitializing = 1,
  kReady = 2,
  kError = 3,
  kShuttingDown = 4,
};

/// @brief Backend capabilities flags.
struct BackendCapabilities {
  bool supports_jni = false;
  bool supports_network = false;
  bool supports_many_clients = false;
};

/// @brief Get human-readable name for BackendStatus.
/// @param status Status to convert
/// @return Human-readable status name
[[nodiscard]] constexpr std::string_view ToString(
    BackendStatus status) noexcept {
  switch (status) {
    case BackendStatus::kUninitialized:
      return "Uninitialized";
    case BackendStatus::kInitializing:
      return "Initializing";
    case BackendStatus::kReady:
      return "Ready";
    case BackendStatus::kError:
      return "Error";
    case BackendStatus::kShuttingDown:
      return "ShuttingDown";
  }
  return "Unknown";
}

}  // namespace roscraft::bridge::common
