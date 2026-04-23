#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/handler.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/node.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

#include <memory_resource>
#include <string>
#include <vector>

namespace roscraft::bridge {

// ─────────────────────────────────────────────────────────────────────────────
// NodeInfoHandler — NodeInfo (receive) / NodeInfoResponse (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `NodeInfoPacket` (receive) and `NodeInfoResponseCmd` (drain).
struct NodeInfoHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::NodeInfoPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  /// @brief Creates a `NodeInfoHandler` with tokens bound to the given queues.
  /// @param in Incoming command queue (produces `NodeInfoCmd`)
  /// @param out Outgoing command queue (consumes `NodeInfoResponseCmd`)
  /// @return Fully initialized handler
  [[nodiscard]] static NodeInfoHandler From(CommandQueue& in,
                                            CommandQueue& out) {
    return {in.MakeProducerToken<NodeInfoCmd>(),
            out.MakeConsumerToken<NodeInfoResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

inline void NodeInfoHandler::Receive(CommandQueue& in,
                                     const fbs::BridgePacket& pkt,
                                     std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_NodeInfoPacket();

  NodeInfoCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.node_name = std::pmr::string(inner->node_name()->string_view(), mr);
  cmd.include_hidden = inner->include_hidden();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void NodeInfoHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<NodeInfoResponseCmd>();

  std::vector<flatbuffers::Offset<fbs::TopicEntry>> publishers;
  std::vector<flatbuffers::Offset<fbs::TopicEntry>> subscribers;
  std::vector<flatbuffers::Offset<fbs::ServiceEntry>> services;

  NodeInfoResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    publishers.clear();
    subscribers.clear();
    services.clear();
    publishers.reserve(cmd.publishers.size());
    subscribers.reserve(cmd.subscribers.size());
    services.reserve(cmd.services.size());

    for (const auto& pub : cmd.publishers) {
      publishers.push_back(fbs::CreateTopicEntry(
          fbb, fbb.CreateString(pub.name), fbb.CreateString(pub.type)));
    }
    for (const auto& sub : cmd.subscribers) {
      subscribers.push_back(fbs::CreateTopicEntry(
          fbb, fbb.CreateString(sub.name), fbb.CreateString(sub.type)));
    }
    for (const auto& svc : cmd.services) {
      services.push_back(fbs::CreateServiceEntry(
          fbb, fbb.CreateString(svc.name), fbb.CreateString(svc.type)));
    }

    const auto node_name = fbb.CreateString(cmd.node_name);
    const auto publishers_offset = fbb.CreateVector(publishers);
    const auto subscribers_offset = fbb.CreateVector(subscribers);
    const auto services_offset = fbb.CreateVector(services);

    fbs::NodeInfoResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_node_name(node_name);
    builder.add_publishers(publishers_offset);
    builder.add_subscribers(subscribers_offset);
    builder.add_services(services_offset);
    builder.add_found(cmd.found);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::NodeInfoResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

}  // namespace roscraft::bridge
