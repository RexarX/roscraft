#pragma once

#include <roscraft/assert.hpp>
#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/network/command/handler.hpp>
#include <roscraft/bridge/network/command/handler_registry.hpp>
#include <roscraft/bridge/network/transport.hpp>
#include <roscraft/generated/bridge_packets_generated.hpp>

#include <rclcpp/logging.hpp>

#include <flatbuffers/flatbuffers.h>

#include <concepts>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <string>
#include <vector>

namespace roscraft::bridge {

namespace network {

/// @brief Serializes `fbb` and sends to all clients via transport.
/// @param transport Transport endpoint
/// @param fbb The `FlatBufferBuilder` containing the serialized data
inline void SendFbb(UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb) {
  const auto* data_ptr = fbb.GetBufferPointer();
  const auto data_size = static_cast<size_t>(fbb.GetSize());
  transport.Send(std::span<const uint8_t>(data_ptr, data_size));
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler: QueryGraph / GraphSnapshot
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles QueryGraphPacket (receive) and GraphSnapshotCmd (send).
struct GraphHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::QueryGraphPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  void DrainAndSend(CommandQueue& out, UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static GraphHandler From(CommandQueue& in, CommandQueue& out) {
    return {in.MakeProducerToken<QueryGraphCmd>(),
            out.MakeConsumerToken<GraphSnapshotCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

inline void GraphHandler::Receive(CommandQueue& in,
                                  const fbs::BridgePacket& pkt,
                                  std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  const auto* inner = pkt.payload_as_QueryGraphPacket();
  QueryGraphCmd cmd{
      .request_id = inner->request_id(),
  };
  in.Enqueue(in_producer, std::move(cmd));
}

inline void GraphHandler::DrainAndSend(CommandQueue& out,
                                       UdpTransport& transport,
                                       flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<GraphSnapshotCmd>();

  std::vector<flatbuffers::Offset<flatbuffers::String>> topics;
  std::vector<flatbuffers::Offset<flatbuffers::String>> services;
  std::vector<flatbuffers::Offset<flatbuffers::String>> actions;

  GraphSnapshotCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    topics.clear();
    services.clear();
    actions.clear();
    topics.reserve(cmd.topics.size());
    services.reserve(cmd.services.size());
    actions.reserve(cmd.actions.size());

    for (const auto& topic : cmd.topics) {
      topics.push_back(fbb.CreateString(topic));
    }

    for (const auto& service : cmd.services) {
      services.push_back(fbb.CreateString(service));
    }

    for (const auto& action : cmd.actions) {
      actions.push_back(fbb.CreateString(action));
    }

    const auto topics_offset = fbb.CreateVector(topics);
    const auto services_offset = fbb.CreateVector(services);
    const auto actions_offset = fbb.CreateVector(actions);

    fbs::GraphSnapshotPacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_topics(topics_offset);
    builder.add_services(services_offset);
    builder.add_actions(actions_offset);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::GraphSnapshotPacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    SendFbb(transport, fbb);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler: SubscribeTopic (receive only — response is TopicPayload push)
// ─────────────────────────────────────────────────────────────────────────────

struct SubscribeTopicHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::SubscribeTopicPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  [[nodiscard]] static SubscribeTopicHandler From(CommandQueue& in) {
    return {in.MakeProducerToken<SubscribeTopicCmd>()};
  }

  CommandQueueProducerToken in_producer;
};

inline void SubscribeTopicHandler::Receive(
    CommandQueue& in, const fbs::BridgePacket& pkt,
    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  // Use the default PMR resource — these strings are enqueued and outlive the
  // per-datagram arena which is reset immediately after DispatchReceive
  // returns.
  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_SubscribeTopicPacket();
  SubscribeTopicCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.topic_name = std::pmr::string(inner->topic_name()->string_view(), mr);
  cmd.message_type = std::pmr::string(inner->message_type()->string_view(), mr);
  in.Enqueue(in_producer, std::move(cmd));
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler: PublishMessage (receive only)
// ─────────────────────────────────────────────────────────────────────────────

struct PublishMessageHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::PublishMessagePacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  [[nodiscard]] static PublishMessageHandler From(CommandQueue& in) {
    return {in.MakeProducerToken<PublishMessageCmd>()};
  }

  CommandQueueProducerToken in_producer;
};

inline void PublishMessageHandler::Receive(
    CommandQueue& in, const fbs::BridgePacket& pkt,
    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  // Use the default PMR resource — these strings/vectors are enqueued and
  // outlive the per-datagram arena which is reset immediately after
  // DispatchReceive returns.
  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_PublishMessagePacket();
  PublishMessageCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.topic_name = std::pmr::string(inner->topic_name()->string_view(), mr);
  cmd.message_type = std::pmr::string(inner->message_type()->string_view(), mr);
  if (const auto* payload = inner->payload()) {
    cmd.payload.assign(payload->data(), payload->data() + payload->size());
  }
  in.Enqueue(in_producer, std::move(cmd));
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler: QueryPlayers / PlayerList
// ─────────────────────────────────────────────────────────────────────────────

struct PlayerListHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::QueryPlayersPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  void DrainAndSend(CommandQueue& out, UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static PlayerListHandler From(CommandQueue& in,
                                              CommandQueue& out) {
    return {in.MakeProducerToken<QueryPlayersCmd>(),
            out.MakeConsumerToken<PlayerListCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

inline void PlayerListHandler::Receive(CommandQueue& in,
                                       const fbs::BridgePacket& pkt,
                                       std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  const auto* inner = pkt.payload_as_QueryPlayersPacket();
  QueryPlayersCmd cmd{.request_id = inner->request_id()};
  in.Enqueue(in_producer, std::move(cmd));
}

inline void PlayerListHandler::DrainAndSend(
    CommandQueue& out, UdpTransport& transport,
    flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<PlayerListCmd>();

  std::vector<flatbuffers::Offset<fbs::PlayerEntry>> entries;

  PlayerListCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    entries.clear();
    entries.reserve(cmd.players.size());

    for (const auto& player : cmd.players) {
      entries.push_back(fbs::CreatePlayerEntry(
          fbb, fbb.CreateString(player.name), player.x, player.y, player.z));
    }

    const auto players_offset = fbb.CreateVector(entries);

    fbs::PlayerListPacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_players(players_offset);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::PlayerListPacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    SendFbb(transport, fbb);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler: TopicPayload (send only — subscription relay)
// ─────────────────────────────────────────────────────────────────────────────

struct TopicPayloadHandler {
  void DrainAndSend(CommandQueue& out, UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static TopicPayloadHandler From(CommandQueue& out) {
    return {out.MakeConsumerToken<TopicPayloadCmd>()};
  }

  CommandQueueConsumerToken out_consumer;
};

inline void TopicPayloadHandler::DrainAndSend(
    CommandQueue& out, UdpTransport& transport,
    flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<TopicPayloadCmd>();

  TopicPayloadCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto topic_name_offset = fbb.CreateString(cmd.topic_name);
    const auto message_type_offset = fbb.CreateString(cmd.message_type);
    const auto payload_offset =
        fbb.CreateVector(cmd.payload.data(), cmd.payload.size());

    fbs::TopicPayloadPacketBuilder builder(fbb);
    builder.add_topic_name(topic_name_offset);
    builder.add_message_type(message_type_offset);
    builder.add_payload(payload_offset);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::TopicPayloadPacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    SendFbb(transport, fbb);
  }
}

/// @brief Dispatches a receive operation to the appropriate handler based on
/// the packet payload type.
inline void DispatchReceive(CommandHandlerRegistry& registry, CommandQueue& in,
                            const fbs::BridgePacket& pkt,
                            std::pmr::memory_resource& arena) {
  switch (pkt.payload_type()) {
    case fbs::PacketPayload::QueryGraphPacket:
      registry.Receive<GraphHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::SubscribeTopicPacket:
      registry.Receive<SubscribeTopicHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::PublishMessagePacket:
      registry.Receive<PublishMessageHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::QueryPlayersPacket:
      registry.Receive<PlayerListHandler>(in, pkt, arena);
      return;
    default:
      return;
  }
}

/// @brief Type list of all handlers that implement `DrainAndSend`.
/// Used to instantiate `CommandHandlerRegistry::DrainAndSendAll`.
using DrainAndSendHandlerTypes =
    std::tuple<GraphHandler, PlayerListHandler, TopicPayloadHandler>;

}  // namespace network

}  // namespace roscraft::bridge
