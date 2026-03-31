#include "../include/roscraft/bridge/jni/bridge_jni.hpp"

namespace roscraft::bridge::jni {

void BridgeJni::Initialize() noexcept {
  initialized_ = true;
}

auto BridgeJni::Initialized() const noexcept -> bool {
  return initialized_;
}

}  // namespace roscraft::bridge::jni
