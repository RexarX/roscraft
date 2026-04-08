#pragma once

#include <concepts>
#include <cstdint>
#include <string_view>

namespace roscraft::bridge {

class App;

/// @brief Bridge status.
enum class BridgeStatus : uint8_t {
  kUninitialized,
  kInitializing,
  kReady,
  kError,
  kShuttingDown,
};

/// @brief Get human-readable name for `BridgeStatus`.
/// @param status Status to convert
/// @return Human-readable status name
[[nodiscard]] constexpr std::string_view ToString(
    BridgeStatus status) noexcept {
  switch (status) {
    case BridgeStatus::kUninitialized:
      return "Uninitialized";
    case BridgeStatus::kInitializing:
      return "Initializing";
    case BridgeStatus::kReady:
      return "Ready";
    case BridgeStatus::kError:
      return "Error";
    case BridgeStatus::kShuttingDown:
      return "ShuttingDown";
  }
  return "Unknown";
}

/// @brief Abstract base class for bridge implementations.
class Bridge {
public:
  virtual ~Bridge() = default;

  /// @brief Initializes the bridge.
  /// @param app Application instance
  virtual void Init(App& app) = 0;

  /// @brief Destroys the bridge.
  /// @param app Application instance
  virtual void Destroy(App& app) = 0;

  /// @brief Reloads the bridge.
  /// @param app Application instance
  virtual void Reload(App& app) = 0;

  /// @brief Runs one bridge tick.
  /// @details Called by the application runner loop.
  /// @param app Application instance
  virtual void Tick(App& app) = 0;

  /// @brief Get the bridge status.
  /// @return Current bridge status
  [[nodiscard]] virtual BridgeStatus Status() const noexcept = 0;
};

/// @brief Concept for bridge implementations.
/// @details Checks if a type is derived from `Bridge`.
/// @tparam T Type to check
template <typename T>
concept BridgeTrait = std::derived_from<T, Bridge>;

}  // namespace roscraft::bridge
