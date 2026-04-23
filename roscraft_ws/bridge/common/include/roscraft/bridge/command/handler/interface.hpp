#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/handler.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/interface.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

#include <memory_resource>
#include <string>
#include <vector>

namespace roscraft::bridge {

// ─────────────────────────────────────────────────────────────────────────────
// InterfaceListHandler — InterfaceList (receive) / InterfaceListResponse (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `InterfaceListPacket` (receive) and
/// `InterfaceListResponseCmd` (drain).
struct InterfaceListHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::InterfaceListPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static InterfaceListHandler From(CommandQueue& in,
                                                 CommandQueue& out) {
    return {in.MakeProducerToken<InterfaceListCmd>(),
            out.MakeConsumerToken<InterfaceListResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// InterfaceShowHandler — InterfaceShow (receive) / InterfaceShowResponse (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `InterfaceShowPacket` (receive) and
/// `InterfaceShowResponseCmd` (drain).
struct InterfaceShowHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::InterfaceShowPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static InterfaceShowHandler From(CommandQueue& in,
                                                 CommandQueue& out) {
    return {in.MakeProducerToken<InterfaceShowCmd>(),
            out.MakeConsumerToken<InterfaceShowResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

inline void InterfaceListHandler::Receive(
    CommandQueue& in, const fbs::BridgePacket& pkt,
    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  const auto* inner = pkt.payload_as_InterfaceListPacket();
  InterfaceListCmd cmd{
      .request_id = inner->request_id(),
      .include_messages = inner->include_messages(),
      .include_services = inner->include_services(),
      .include_actions = inner->include_actions(),
  };
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void InterfaceListHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<InterfaceListResponseCmd>();

  std::vector<flatbuffers::Offset<flatbuffers::String>> messages;
  std::vector<flatbuffers::Offset<flatbuffers::String>> services;
  std::vector<flatbuffers::Offset<flatbuffers::String>> actions;

  InterfaceListResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    messages.clear();
    services.clear();
    actions.clear();
    messages.reserve(cmd.messages.size());
    services.reserve(cmd.services.size());
    actions.reserve(cmd.actions.size());

    for (const auto& msg : cmd.messages) {
      messages.push_back(fbb.CreateString(msg));
    }
    for (const auto& svc : cmd.services) {
      services.push_back(fbb.CreateString(svc));
    }
    for (const auto& act : cmd.actions) {
      actions.push_back(fbb.CreateString(act));
    }

    const auto messages_offset = fbb.CreateVector(messages);
    const auto services_offset = fbb.CreateVector(services);
    const auto actions_offset = fbb.CreateVector(actions);

    fbs::InterfaceListResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_messages(messages_offset);
    builder.add_services(services_offset);
    builder.add_actions(actions_offset);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::InterfaceListResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

inline void InterfaceShowHandler::Receive(
    CommandQueue& in, const fbs::BridgePacket& pkt,
    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_InterfaceShowPacket();

  InterfaceShowCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.interface_type =
      std::pmr::string(inner->interface_type()->string_view(), mr);
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void InterfaceShowHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<InterfaceShowResponseCmd>();

  InterfaceShowResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto interface_type = fbb.CreateString(cmd.interface_type);
    const auto definition = fbb.CreateString(cmd.definition);

    fbs::InterfaceShowResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_interface_type(interface_type);
    builder.add_definition(definition);
    builder.add_found(cmd.found);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::InterfaceShowResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

}  // namespace roscraft::bridge
