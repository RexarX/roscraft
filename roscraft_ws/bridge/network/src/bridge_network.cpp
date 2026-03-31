#include <roscraft/bridge/network/bridge_network.hpp>

#include <cstdint>

namespace roscraft::bridge::network {

void BridgeNetwork::Listen(const uint16_t port) noexcept {
  port_ = port;
}

auto BridgeNetwork::Port() const noexcept -> uint16_t {
  return port_;
}

}  // namespace roscraft::bridge::network
