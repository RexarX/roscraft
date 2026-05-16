#include <pch.hpp>

#include <roscraft/bridge/jni/native_methods.hpp>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/app/config.hpp>
#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/jni/bridge.hpp>
#include <roscraft/bridge/jni/config.hpp>
#include <roscraft/bridge/jni/env_setup.hpp>

#include <rclcpp/logging.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace {

struct JByteArrayView {
  std::span<const uint8_t> data;
  jbyte* bytes = nullptr;
};

[[nodiscard]] JByteArrayView AcquireJByteArrayView(JNIEnv* env,
                                                   jbyteArray array) {
  ROSCRAFT_ASSERT(env != nullptr, "JNIEnv is null!");

  if (array == nullptr) [[unlikely]] {
    return {};
  }

  const auto len = env->GetArrayLength(array);
  if (len <= 0) {
    return {};
  }

  auto* bytes = env->GetByteArrayElements(array, nullptr);
  if (bytes == nullptr) [[unlikely]] {
    return {};
  }

  return {
      .data = std::span{std::launder(reinterpret_cast<const uint8_t*>(bytes)),
                        static_cast<size_t>(len)},
      .bytes = bytes,
  };
}

void ReleaseJByteArrayView(JNIEnv* env, jbyteArray array,
                           const JByteArrayView& view) noexcept {
  ROSCRAFT_ASSERT(env != nullptr, "JNIEnv is null!");

  if (array == nullptr) [[unlikely]] {
    return;
  }

  if (view.bytes == nullptr) [[unlikely]] {
    return;
  }

  env->ReleaseByteArrayElements(array, view.bytes, JNI_ABORT);
}

}  // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_net_roscraft_bridge_JniBridge_nativeCreate(JNIEnv* env, jclass /*cls*/) {
  ROSCRAFT_ASSERT(env != nullptr, "JNIEnv is null!");

  JavaVM* jvm = nullptr;
  if (env->GetJavaVM(&jvm) != JNI_OK || jvm == nullptr) {
    RCLCPP_ERROR(rclcpp::get_logger("JniBridge"),
                 "nativeCreate: failed to obtain JavaVM!");
    return JNI_FALSE;
  }

  auto& app = roscraft::bridge::App::Instance();
  if (app.State() == roscraft::bridge::AppState::kInitialized) {
    return JNI_TRUE;
  }

  roscraft::bridge::jni::SetupRosEnvironment();

  auto bridge = std::make_unique<roscraft::bridge::jni::JniBridge>(
      roscraft::bridge::jni::BridgeConfig{.jvm = jvm});
  app.Init(roscraft::bridge::AppConfig::From(std::move(bridge)));

  return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_net_roscraft_bridge_JniBridge_nativeDestroy(
    JNIEnv* /*env*/, jclass /*cls*/) {
  auto& app = roscraft::bridge::App::Instance();
  if (app.State() == roscraft::bridge::AppState::kInitialized) {
    app.Shutdown();
  }
}

JNIEXPORT void JNICALL
Java_net_roscraft_bridge_JniBridge_nativeRegisterCallback(JNIEnv* env,
                                                          jclass /*cls*/,
                                                          jobject callback) {
  ROSCRAFT_ASSERT(env != nullptr, "JNIEnv is null!");

  auto& app = roscraft::bridge::App::Instance();
  auto& bridge = app.GetBridge<roscraft::bridge::jni::JniBridge>();
  bridge.RegisterCallback(env, callback);
}

JNIEXPORT void JNICALL
Java_net_roscraft_bridge_JniBridge_nativeTick(JNIEnv* /*env*/, jclass /*cls*/) {
  roscraft::bridge::App::Instance().Tick();
}

JNIEXPORT void JNICALL Java_net_roscraft_bridge_JniBridge_nativeSendPacket(
    JNIEnv* env, jclass /*cls*/, jbyteArray packet) {
  ROSCRAFT_ASSERT(env != nullptr, "JNIEnv is null!");

  auto& app = roscraft::bridge::App::Instance();
  auto& bridge = app.GetBridge<roscraft::bridge::jni::JniBridge>();

  const auto bytes = AcquireJByteArrayView(env, packet);
  bridge.ReceivePacket(bytes.data);
  ReleaseJByteArrayView(env, packet, bytes);
}

}  // extern "C"
