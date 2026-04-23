#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/handler.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/service.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

#include <memory_resource>
#include <string>
#include <vector>

namespace roscraft::bridge {

// ─────────────────────────────────────────────────────────────────────────────
// ServiceInfoHandler — ServiceInfo (receive) / ServiceInfoResponse (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `ServiceInfoPacket` (receive) and `ServiceInfoResponseCmd`
/// (drain).
struct ServiceInfoHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::ServiceInfoPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ServiceInfoHandler From(CommandQueue& in,
                                               CommandQueue& out) {
    return {in.MakeProducerToken<ServiceInfoCmd>(),
            out.MakeConsumerToken<ServiceInfoResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// ServiceCallHandler — ServiceCall (receive) / ServiceCallResponse (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `ServiceCallPacket` (receive) and `ServiceCallResponseCmd`
/// (drain).
struct ServiceCallHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::ServiceCallPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ServiceCallHandler From(CommandQueue& in,
                                               CommandQueue& out) {
    return {in.MakeProducerToken<ServiceCallCmd>(),
            out.MakeConsumerToken<ServiceCallResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

inline void ServiceInfoHandler::Receive(CommandQueue& in,
                                        const fbs::BridgePacket& pkt,
                                        std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_ServiceInfoPacket();

  ServiceInfoCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.service_name = std::pmr::string(inner->service_name()->string_view(), mr);
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void ServiceInfoHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ServiceInfoResponseCmd>();

  std::vector<flatbuffers::Offset<flatbuffers::String>> client_nodes;
  std::vector<flatbuffers::Offset<flatbuffers::String>> server_nodes;

  ServiceInfoResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    client_nodes.clear();
    server_nodes.clear();
    client_nodes.reserve(cmd.client_nodes.size());
    server_nodes.reserve(cmd.server_nodes.size());

    for (const auto& node : cmd.client_nodes) {
      client_nodes.push_back(fbb.CreateString(node));
    }
    for (const auto& node : cmd.server_nodes) {
      server_nodes.push_back(fbb.CreateString(node));
    }

    const auto service_name = fbb.CreateString(cmd.service_name);
    const auto service_type = fbb.CreateString(cmd.service_type);
    const auto client_nodes_offset = fbb.CreateVector(client_nodes);
    const auto server_nodes_offset = fbb.CreateVector(server_nodes);

    fbs::ServiceInfoResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_service_name(service_name);
    builder.add_service_type(service_type);
    builder.add_client_count(cmd.client_count);
    builder.add_server_count(cmd.server_count);
    builder.add_client_nodes(client_nodes_offset);
    builder.add_server_nodes(server_nodes_offset);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ServiceInfoResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

inline void ServiceCallHandler::Receive(CommandQueue& in,
                                        const fbs::BridgePacket& pkt,
                                        std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_ServiceCallPacket();

  ServiceCallCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.service_name = std::pmr::string(inner->service_name()->string_view(), mr);
  cmd.service_type = std::pmr::string(inner->service_type()->string_view(), mr);
  if (const auto* payload = inner->payload()) {
    cmd.payload.assign(payload->data(), payload->data() + payload->size());
  }
  cmd.timeout_seconds = inner->timeout_seconds();
  cmd.repeat_count = inner->repeat_count();
  cmd.rate_hz = inner->rate_hz();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void ServiceCallHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ServiceCallResponseCmd>();

  ServiceCallResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto service_name = fbb.CreateString(cmd.service_name);
    const auto service_type = fbb.CreateString(cmd.service_type);
    const auto response_payload = fbb.CreateVector(cmd.response_payload.data(),
                                                   cmd.response_payload.size());
    const auto result_text = fbb.CreateString(cmd.result_text);

    fbs::ServiceCallResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_service_name(service_name);
    builder.add_service_type(service_type);
    builder.add_success(cmd.success);
    builder.add_response_payload(response_payload);
    builder.add_result_text(result_text);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ServiceCallResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

}  // namespace roscraft::bridge
