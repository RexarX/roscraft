#include <roscraft/bridge/jni/env_guard.hpp>

#include <jni.h>

namespace roscraft::bridge::jni {

JniEnvGuard::JniEnvGuard(JavaVM* jvm) noexcept : jvm_(jvm) {
  if (jvm_ == nullptr) [[unlikely]] {
    return;
  }

  void* raw = nullptr;
  const jint get_rc = jvm_->GetEnv(&raw, JNI_VERSION_1_8);

  if (get_rc == JNI_OK) [[likely]] {
    env_ = static_cast<JNIEnv*>(raw);
    result_ = AttachResult::kAlreadyAttached;
    return;
  }

  JavaVMAttachArgs args{
      .version = JNI_VERSION_1_8,
      .name = nullptr,
      .group = nullptr,
  };
  const jint attach_rc = jvm_->AttachCurrentThread(&raw, &args);
  if (attach_rc == JNI_OK) [[likely]] {
    env_ = static_cast<JNIEnv*>(raw);
    result_ = AttachResult::kAttached;
  }
  // else env_ remains nullptr and result_ remains kFailed
}

JniEnvGuard::~JniEnvGuard() noexcept {
  if (result_ == AttachResult::kAttached && jvm_ != nullptr) [[likely]] {
    jvm_->DetachCurrentThread();
  }
}

}  // namespace roscraft::bridge::jni
