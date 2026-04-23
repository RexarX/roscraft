#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/handler.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/graph.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

#include <memory_resource>
#include <vector>

namespace roscraft::bridge {

// ─────────────────────────────────────────────────────────────────────────────
// GraphHandler — QueryGraph (receive) / GraphSnapshot (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `QueryGraphPacket` (receive) and `GraphSnapshotCmd` (drain).
struct GraphHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::QueryGraphPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  /// @brief Creates a `GraphHandler` with tokens bound to the given queues.
  /// @param in Incoming command queue (produces `QueryGraphCmd`)
  /// @param out Outgoing command queue (consumes `GraphSnapshotCmd`)
  /// @return Fully initialized handler
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
  QueryGraphCmd cmd{.request_id = inner->request_id()};
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void GraphHandler::DrainAndFlush(CommandQueue& out, Sink& sink,
                                        flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<GraphSnapshotCmd>();

  std::vector<flatbuffers::Offset<fbs::NodeEntry>> nodes;
  std::vector<flatbuffers::Offset<fbs::TopicEntry>> topics;
  std::vector<flatbuffers::Offset<fbs::ServiceEntry>> services;
  std::vector<flatbuffers::Offset<fbs::ActionEntry>> actions;

  GraphSnapshotCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    nodes.clear();
    topics.clear();
    services.clear();
    actions.clear();
    nodes.reserve(cmd.nodes.size());
    topics.reserve(cmd.topics.size());
    services.reserve(cmd.services.size());
    actions.reserve(cmd.actions.size());

    for (const auto& node : cmd.nodes) {
      nodes.push_back(fbs::CreateNodeEntry(fbb, fbb.CreateString(node.name)));
    }
    for (const auto& topic : cmd.topics) {
      topics.push_back(fbs::CreateTopicEntry(fbb, fbb.CreateString(topic.name),
                                             fbb.CreateString(topic.type)));
    }
    for (const auto& service : cmd.services) {
      services.push_back(fbs::CreateServiceEntry(
          fbb, fbb.CreateString(service.name), fbb.CreateString(service.type)));
    }
    for (const auto& action : cmd.actions) {
      actions.push_back(fbs::CreateActionEntry(
          fbb, fbb.CreateString(action.name), fbb.CreateString(action.type)));
    }

    const auto nodes_offset = fbb.CreateVector(nodes);
    const auto topics_offset = fbb.CreateVector(topics);
    const auto services_offset = fbb.CreateVector(services);
    const auto actions_offset = fbb.CreateVector(actions);

    fbs::GraphSnapshotPacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_nodes(nodes_offset);
    builder.add_topics(topics_offset);
    builder.add_services(services_offset);
    builder.add_actions(actions_offset);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::GraphSnapshotPacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

}  // namespace roscraft::bridge
