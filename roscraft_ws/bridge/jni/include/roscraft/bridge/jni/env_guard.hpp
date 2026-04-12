#pragma once

#include <jni.h>

#include <cstdint>

namespace roscraft::bridge::jni {

/// @brief Result of attaching a thread to the JVM.
enum class AttachResult : uint8_t {
  kAlreadyAttached,  ///< Thread was already attached; caller must not detach
  kAttached,         ///< Thread was freshly attached; caller must detach
  kFailed,           ///< Attach failed
};

/// @brief RAII guard that attaches the current thread to the JVM on
/// construction and detaches it (if needed) on destruction.
/// @details If the thread is already attached when the guard is created,
/// it is **not** detached on destruction, preserving the caller's
/// attachment state.
///
/// `Env()` returns `nullptr` when attachment failed; always check
/// before use.
class JniEnvGuard {
public:
  /// @brief Attach the current thread to `jvm`.
  /// @param jvm JVM instance (must not be null)
  explicit JniEnvGuard(JavaVM* jvm) noexcept;
  ~JniEnvGuard() noexcept;

  /// @brief Gets the JNI environment for the current thread.
  /// @return Pointer to `JNIEnv`, or `nullptr` if attachment failed
  [[nodiscard]] JNIEnv* Env() const noexcept { return env_; }

  /// @brief Checks if the environment is valid (i.e., the thread is attached).
  /// @return `true` if the environment is valid, `false` otherwise
  [[nodiscard]] bool Valid() const noexcept { return env_ != nullptr; }

private:
  JavaVM* jvm_ = nullptr;
  JNIEnv* env_ = nullptr;
  AttachResult result_ = AttachResult::kFailed;
};

}  // namespace roscraft::bridge::jni
