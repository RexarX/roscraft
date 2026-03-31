#pragma once

namespace roscraft::bridge::jni {

class BridgeJni {
public:
  BridgeJni() = default;
  ~BridgeJni() = default;

  BridgeJni(const BridgeJni&) = default;
  BridgeJni(BridgeJni&&) noexcept = default;
  auto operator=(const BridgeJni&) -> BridgeJni& = default;
  auto operator=(BridgeJni&&) noexcept -> BridgeJni& = default;

  void Initialize() noexcept;

  [[nodiscard]] auto Initialized() const noexcept -> bool;

private:
  bool initialized_ = false;
};

}  // namespace roscraft::bridge::jni
