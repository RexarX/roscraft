#pragma once

#include <roscraft/bridge/network/udp_transport.hpp>

#include <flatbuffers/flatbuffers.h>

#include <cstddef>
#include <cstdint>
#include <span>

namespace roscraft::bridge::network {

/// @brief `PacketSink` implementation that sends a completed FlatBuffers
/// buffer to all currently registered UDP clients.
/// @details Wraps a `UdpTransport` reference. Satisfies `PacketSink`.
class UdpPacketSink {
public:
  /// @brief Constructs a sink backed by the given transport.
  /// @param transport UDP transport to send through
  explicit UdpPacketSink(UdpTransport& transport) : transport_(transport) {}

  /// @brief Extracts the finished buffer from `fbb` and sends it via UDP.
  /// @param fbb Completed FlatBufferBuilder (must have called `Finish*`)
  void Send(flatbuffers::FlatBufferBuilder& fbb);

private:
  UdpTransport& transport_;
};

inline void UdpPacketSink::Send(flatbuffers::FlatBufferBuilder& fbb) {
  const auto* ptr = fbb.GetBufferPointer();
  const auto size = static_cast<size_t>(fbb.GetSize());
  transport_.Send(std::span<const uint8_t>(ptr, size));
}

}  // namespace roscraft::bridge::network
