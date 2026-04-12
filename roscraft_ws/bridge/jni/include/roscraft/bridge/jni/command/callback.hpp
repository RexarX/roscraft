#pragma once

#include <roscraft/bridge/assert.hpp>

#include <jni.h>

#include <cstdint>
#include <limits>
#include <span>

namespace roscraft::bridge::jni {

class BridgeCallback {
public:
  BridgeCallback() = default;
  BridgeCallback(const BridgeCallback&) = delete;
  BridgeCallback(BridgeCallback&&) = delete;
  ~BridgeCallback() = default;

  BridgeCallback& operator=(const BridgeCallback&) = delete;
  BridgeCallback& operator=(BridgeCallback&&) = delete;

  /// @brief Initializes callback object and caches Java method IDs.
  /// @warning Triggers assertion if `env` or `obj` is null.
  /// @param env JNI environment
  /// @param obj Java callback object
  void Init(JNIEnv* env, jobject obj) noexcept;

  /// @brief Releases JNI global references and cached method IDs.
  /// @param env JNI environment
  void Destroy(JNIEnv* env) noexcept;

  /// @brief Sends one FlatBuffers packet to Java callback.
  /// @param env JNI environment
  /// @param packet Serialized packet bytes
  void SendPacket(JNIEnv* env, std::span<const uint8_t> packet) const noexcept;

  /// @brief Checks if the callback is valid (i.e., the object and method IDs
  /// are set).
  /// @return `true` if the callback is valid, `false` otherwise
  [[nodiscard]] bool Valid() const noexcept {
    return obj_ != nullptr && on_packet_ != nullptr;
  }

private:
  jobject obj_ = nullptr;
  jmethodID on_packet_ = nullptr;
};

inline void BridgeCallback::Init(JNIEnv* env, jobject obj) noexcept {
  ROSCRAFT_ASSERT(env != nullptr, "JNIEnv is null!");
  ROSCRAFT_ASSERT(obj != nullptr, "Callback object is null!");

  if (obj_ != nullptr) {
    Destroy(env);
  }

  obj_ = env->NewGlobalRef(obj);
  ROSCRAFT_ASSERT(obj_ != nullptr, "Failed to create callback global ref!");

  const auto cls = env->GetObjectClass(obj_);
  ROSCRAFT_ASSERT(cls != nullptr, "Failed to get callback class!");

  on_packet_ = env->GetMethodID(cls, "onPacket", "([B)V");
  ROSCRAFT_ASSERT(on_packet_ != nullptr,
                  "Failed to find callback method 'onPacket([B)V'!");

  env->DeleteLocalRef(cls);
}

inline void BridgeCallback::Destroy(JNIEnv* env) noexcept {
  ROSCRAFT_ASSERT(env != nullptr, "JNIEnv is null!");

  if (obj_ != nullptr) {
    env->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }

  on_packet_ = nullptr;
}

inline void BridgeCallback::SendPacket(
    JNIEnv* env, std::span<const uint8_t> packet) const noexcept {
  ROSCRAFT_ASSERT(env != nullptr, "JNIEnv is null!");
  ROSCRAFT_ASSERT(Valid(), "Bridge callback is not initialized!");

  constexpr auto kJSizeMax =
      static_cast<size_t>(std::numeric_limits<jsize>::max());
  ROSCRAFT_ASSERT(packet.size() <= kJSizeMax, "Packet is too big for JNI!");

  const auto size = static_cast<jsize>(packet.size());
  auto* data = reinterpret_cast<const jbyte*>(packet.data());

  const auto jpacket = env->NewByteArray(size);
  if (jpacket == nullptr) [[unlikely]] {
    return;
  }

  if (size > 0) {
    env->SetByteArrayRegion(jpacket, 0, size, data);
  }
  env->CallVoidMethod(obj_, on_packet_, jpacket);
  env->DeleteLocalRef(jpacket);
}

}  // namespace roscraft::bridge::jni
