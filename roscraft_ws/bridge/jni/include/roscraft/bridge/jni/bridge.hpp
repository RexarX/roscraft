#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/bridge.hpp>
#include <roscraft/bridge/jni/config.hpp>

#include <atomic>

namespace roscraft::bridge::jni {

class JNIBridge final : public Bridge {
public:
  /// @brief Construct a new `JNIBridge` object.
  /// @warning Triggers assertion if config is invalid.
  /// @param config JNI bridge configuration
  explicit JNIBridge(BridgeConfig config = {});
  ~JNIBridge() override;

  /// @brief Parse command line arguments.
  /// @param argc Argument count
  /// @param argv Argument values
  void ParseArgs(int argc, char* argv[]);

  /// @brief Initialize the JNI bridge.
  /// @param app Application instance
  void Init(App& app) override;

  /// @brief Destroy the JNI bridge.
  /// @param app Application instance
  void Destroy(App& app) override;

  /// @brief Reloads the JNI bridge.
  /// @param app Application instance
  void Reload(App& app) override;

  /// @brief Runs one bridge tick.
  /// @param app Application instance
  void Tick(App& app) override;

  /// @brief Set the JNI bridge configuration.
  /// @param config JNI bridge configuration
  void SetConfig(BridgeConfig config) noexcept { config_ = config; }

  /// @brief Get the current bridge status.
  /// @return Bridge status
  [[nodiscard]] BridgeStatus Status() const noexcept override {
    return status_.load(std::memory_order_relaxed);
  }

  /// @brief Get the JNI bridge configuration.
  /// @return Const reference to the JNI bridge configuration
  [[nodiscard]] const BridgeConfig& Config() const noexcept { return config_; }

private:
  BridgeConfig config_;

  std::atomic<BridgeStatus> status_{BridgeStatus::kUninitialized};
};

inline JNIBridge::JNIBridge(BridgeConfig config) : config_(config) {
  ROSCRAFT_ASSERT(config.Valid(), "Config is invalid!");
}

inline JNIBridge::~JNIBridge() {
  if (Status() == BridgeStatus::kUninitialized) {
    return;
  }
}

inline void JNIBridge::Reload(App& app) {
  Destroy(app);
  Init(app);
}

}  // namespace roscraft::bridge::jni
