#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/handler.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/addon.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

#include <memory_resource>
#include <string>

namespace roscraft::bridge {

// ─────────────────────────────────────────────────────────────────────────────
// AddonEventHandler — AddonEvent (receive) / AddonEvent (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `AddonEventPacket` (receive) and `AddonEventCmd` (drain).
struct AddonEventHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::AddonEventPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static AddonEventHandler From(CommandQueue& in,
                                              CommandQueue& out) {
    return {in.MakeProducerToken<AddonEventCmd>(),
            out.MakeConsumerToken<AddonEventCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

inline void AddonEventHandler::Receive(CommandQueue& in,
                                       const fbs::BridgePacket& pkt,
                                       std::pmr::memory_resource& arena) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  const auto* inner = pkt.payload_as_AddonEventPacket();
  AddonEventCmd cmd(&arena);
  cmd.request_id = inner->request_id();
  cmd.addon_id = std::pmr::string(inner->addon_id()->string_view(), &arena);
  cmd.event_type = std::pmr::string(inner->event_type()->string_view(), &arena);
  if (const auto* encoding = inner->encoding()) {
    cmd.encoding = std::pmr::string(encoding->string_view(), &arena);
  }
  cmd.response = inner->response();
  if (const auto* payload = inner->payload()) {
    cmd.payload.assign(payload->begin(), payload->end());
  }

  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void AddonEventHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<AddonEventCmd>();

  AddonEventCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto addon_id = fbb.CreateString(cmd.addon_id);
    const auto event_type = fbb.CreateString(cmd.event_type);
    const auto encoding = cmd.encoding.empty()
                              ? flatbuffers::Offset<flatbuffers::String>()
                              : fbb.CreateString(cmd.encoding);
    const auto payload =
        fbb.CreateVector(cmd.payload.data(), cmd.payload.size());

    fbs::AddonEventPacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_addon_id(addon_id);
    builder.add_event_type(event_type);
    if (encoding.o != 0) {
      builder.add_encoding(encoding);
    }
    builder.add_payload(payload);
    builder.add_response(cmd.response);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::AddonEventPacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

}  // namespace roscraft::bridge
