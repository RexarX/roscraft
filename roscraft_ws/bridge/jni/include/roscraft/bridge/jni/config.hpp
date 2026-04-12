#pragma once

#include <jni.h>

namespace roscraft::bridge::jni {

struct BridgeConfig {
  JavaVM* jvm = nullptr;  ///< JVM instance

  /// @brief Checks if the bridge configuration is valid.
  /// @return `true` if the configuration is valid, `false` otherwise.
  [[nodiscard]] constexpr bool Valid() const noexcept { return jvm != nullptr; }
};

}  // namespace roscraft::bridge::jni
