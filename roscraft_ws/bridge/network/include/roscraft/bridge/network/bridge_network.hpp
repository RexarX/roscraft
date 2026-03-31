#pragma once

#include <cstdint>

namespace roscraft::bridge::network {

class BridgeNetwork {
public:
  BridgeNetwork() = default;
  ~BridgeNetwork() = default;

  BridgeNetwork(const BridgeNetwork&) = default;
  BridgeNetwork(BridgeNetwork&&) noexcept = default;
  auto operator=(const BridgeNetwork&) -> BridgeNetwork& = default;
  auto operator=(BridgeNetwork&&) noexcept -> BridgeNetwork& = default;

  void Listen(uint16_t port) noexcept;

  [[nodiscard]] auto Port() const noexcept -> uint16_t;

private:
  uint16_t port_ = 0;
};

}  // namespace roscraft::bridge::network
