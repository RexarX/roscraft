#pragma once

#include <roscraft/bridge/jni/command/callback.hpp>

#include <flatbuffers/flatbuffers.h>

#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>

namespace roscraft::bridge::jni {

/// @brief `PacketSink` implementation that delivers a completed FlatBuffers
/// buffer to the Java bridge callback via JNI.
/// @details Wraps a `JNIEnv*` and a `BridgeCallback` reference. Satisfies
/// `PacketSink`.
class JniPacketSink {
public:
  /// @brief Constructs a sink backed by the given JNI environment and callback.
  /// @param env JNI environment for the calling thread
  /// @param callback Initialized Java callback object
  JniPacketSink(JNIEnv* env, const BridgeCallback& callback)
      : env_(env), callback_(callback) {}

  /// @brief Extracts the finished buffer from `fbb` and sends it via JNI.
  /// @param fbb Completed FlatBufferBuilder (must have called `Finish*`)
  void Send(flatbuffers::FlatBufferBuilder& fbb);

private:
  JNIEnv* env_ = nullptr;
  std::reference_wrapper<const BridgeCallback> callback_;
};

inline void JniPacketSink::Send(flatbuffers::FlatBufferBuilder& fbb) {
  const auto* ptr = fbb.GetBufferPointer();
  const auto size = static_cast<size_t>(fbb.GetSize());
  callback_.get().SendPacket(env_, std::span<const uint8_t>(ptr, size));
}

}  // namespace roscraft::bridge::jni
