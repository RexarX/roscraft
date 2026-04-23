#pragma once

#include <roscraft/bridge/command/handler.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace roscraft::bridge::tests {

struct CollectingSink {
  void Send(flatbuffers::FlatBufferBuilder& fbb) {
    packets.emplace_back(fbb.GetBufferPointer(),
                         fbb.GetBufferPointer() + fbb.GetSize());
  }

  std::vector<std::vector<uint8_t>> packets;
};

static_assert(PacketSink<CollectingSink>);

template <typename BuildPayloadFn>
[[nodiscard]] inline auto BuildPacket(fbs::PacketPayload payload_type,
                                      BuildPayloadFn&& build_payload)
    -> std::vector<uint8_t> {
  flatbuffers::FlatBufferBuilder fbb;
  const auto payload = std::forward<BuildPayloadFn>(build_payload)(fbb);
  const auto root = fbs::CreateBridgePacket(fbb, payload_type, payload.Union());
  fbs::FinishBridgePacketBuffer(fbb, root);

  return {fbb.GetBufferPointer(), fbb.GetBufferPointer() + fbb.GetSize()};
}

[[nodiscard]] inline auto ParsePacket(std::span<const uint8_t> bytes)
    -> const fbs::BridgePacket* {
  return fbs::GetBridgePacket(bytes.data());
}

[[nodiscard]] inline auto ParsePacket(const std::vector<uint8_t>& bytes)
    -> const fbs::BridgePacket* {
  return ParsePacket(std::span<const uint8_t>(bytes.data(), bytes.size()));
}

}  // namespace roscraft::bridge::tests
