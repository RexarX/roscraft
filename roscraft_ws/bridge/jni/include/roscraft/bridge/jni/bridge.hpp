#pragma once

#include <roscraft/bridge/bridge.hpp>
#include <roscraft/bridge/jni/command/callback.hpp>
#include <roscraft/bridge/jni/command/handler_registry.hpp>
#include <roscraft/bridge/jni/config.hpp>

#include <flatbuffers/flatbuffers.h>

#include <jni.h>

#include <atomic>
#include <span>

namespace roscraft::bridge::jni {

class JniBridge final : public Bridge {
public:
  explicit JniBridge(BridgeConfig config = {}) noexcept;
  ~JniBridge() override;

  JniBridge(const JniBridge&) = delete;
  JniBridge(JniBridge&&) = delete;

  JniBridge& operator=(const JniBridge&) = delete;
  JniBridge& operator=(JniBridge&&) = delete;

  /// @brief Initializes the bridge.
  /// @warning Triggers assertion if bridge is already initialized.
  /// @param app Application instance
  void Init(App& app) override;

  /// @brief Destroys the bridge and releases resources.
  /// @param app Application instance
  void Destroy(App& app) override;

  /// @brief Reloads bridge state.
  /// @param app Application instance
  void Reload(App& app) override;

  /// @brief Runs one bridge tick.
  /// @param app Application instance
  void Tick(App& app) override;

  /// @brief Registers Java callback target object.
  /// @warning Triggers assertion if `env` or `callback_obj` is null.
  /// @param env JNI environment
  /// @param callback_obj Java callback object
  void RegisterCallback(JNIEnv* env, jobject callback_obj) noexcept {
    callback_.Init(env, callback_obj);
  }

  /// @brief Dispatches one incoming FlatBuffers packet.
  /// @param packet Serialized packet bytes
  void ReceivePacket(std::span<const uint8_t> packet);

  /// @brief Gets the current bridge status.
  /// @return Current bridge status
  [[nodiscard]] BridgeStatus Status() const noexcept override {
    return status_.load(std::memory_order_relaxed);
  }

  /// @brief Gets the current bridge configuration.
  /// @return Current bridge configuration
  [[nodiscard]] const BridgeConfig& Config() const noexcept { return config_; }

private:
  void InitCommandHandlerRegistry();

  BridgeConfig config_;
  std::atomic<BridgeStatus> status_{BridgeStatus::kUninitialized};

  BridgeCallback callback_;
  CommandHandlerRegistry registry_;
};

inline JniBridge::JniBridge(BridgeConfig config) noexcept : config_(config) {
  ROSCRAFT_ASSERT(config.Valid(), "BridgeConfig is invalid!");
}

inline void JniBridge::Reload(App& app) {
  Destroy(app);
  Init(app);
}

}  // namespace roscraft::bridge::jni
