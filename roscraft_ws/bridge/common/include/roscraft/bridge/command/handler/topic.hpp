#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/handler.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/topic.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

#include <memory_resource>
#include <string>
#include <vector>

namespace roscraft::bridge {

// ─────────────────────────────────────────────────────────────────────────────
// TopicInfoHandler — TopicInfo (receive) / TopicInfoResponse (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `TopicInfoPacket` (receive) and `TopicInfoResponseCmd`
/// (drain).
struct TopicInfoHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::TopicInfoPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static TopicInfoHandler From(CommandQueue& in,
                                             CommandQueue& out) {
    return {in.MakeProducerToken<TopicInfoCmd>(),
            out.MakeConsumerToken<TopicInfoResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// TopicSubscribeHandler — TopicSubscribe (receive only)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `TopicSubscribePacket` (receive only).
/// @details The response is delivered asynchronously via `TopicPayloadHandler`.
struct TopicSubscribeHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::TopicSubscribePacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  [[nodiscard]] static TopicSubscribeHandler From(CommandQueue& in) {
    return {in.MakeProducerToken<TopicSubscribeCmd>()};
  }

  CommandQueueProducerToken in_producer;
};

// ─────────────────────────────────────────────────────────────────────────────
// TopicPublishMessageHandler — PublishMessage (receive only)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `TopicPublishMessagePacket` (receive only).
struct TopicPublishMessageHandler {
  static constexpr auto kReceiveType =
      fbs::PacketPayload::TopicPublishMessagePacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  [[nodiscard]] static TopicPublishMessageHandler From(CommandQueue& in) {
    return {in.MakeProducerToken<TopicPublishMessageCmd>()};
  }

  CommandQueueProducerToken in_producer;
};

// ─────────────────────────────────────────────────────────────────────────────
// TopicHzHandler — TopicHz (receive) / TopicHzResponse (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `TopicHzPacket` (receive) and `TopicHzResponseCmd` (drain).
struct TopicHzHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::TopicHzPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static TopicHzHandler From(CommandQueue& in,
                                           CommandQueue& out) {
    return {in.MakeProducerToken<TopicHzCmd>(),
            out.MakeConsumerToken<TopicHzResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// TopicBwHandler — TopicBw (receive) / TopicBwResponse (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `TopicBwPacket` (receive) and `TopicBwResponseCmd` (drain).
struct TopicBwHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::TopicBwPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static TopicBwHandler From(CommandQueue& in,
                                           CommandQueue& out) {
    return {in.MakeProducerToken<TopicBwCmd>(),
            out.MakeConsumerToken<TopicBwResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// TopicDelayHandler — TopicDelay (receive) / TopicDelayResponse (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `TopicDelayPacket` (receive) and `TopicDelayResponseCmd`
/// (drain).
struct TopicDelayHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::TopicDelayPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static TopicDelayHandler From(CommandQueue& in,
                                              CommandQueue& out) {
    return {in.MakeProducerToken<TopicDelayCmd>(),
            out.MakeConsumerToken<TopicDelayResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// TopicPayloadHandler — TopicPayload (send only — subscription relay)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Drains `TopicPayloadCmd` and sends serialized payloads to clients.
struct TopicPayloadHandler {
  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static TopicPayloadHandler From(CommandQueue& out) {
    return {out.MakeConsumerToken<TopicPayloadCmd>()};
  }

  CommandQueueConsumerToken out_consumer;
};

inline void TopicInfoHandler::Receive(CommandQueue& in,
                                      const fbs::BridgePacket& pkt,
                                      std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_TopicInfoPacket();

  TopicInfoCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.topic_name = std::pmr::string(inner->topic_name()->string_view(), mr);
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void TopicInfoHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<TopicInfoResponseCmd>();

  std::vector<flatbuffers::Offset<flatbuffers::String>> publisher_nodes;
  std::vector<flatbuffers::Offset<flatbuffers::String>> subscriber_nodes;

  TopicInfoResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    publisher_nodes.clear();
    subscriber_nodes.clear();
    publisher_nodes.reserve(cmd.publisher_nodes.size());
    subscriber_nodes.reserve(cmd.subscriber_nodes.size());

    for (const auto& node : cmd.publisher_nodes) {
      publisher_nodes.push_back(fbb.CreateString(node));
    }
    for (const auto& node : cmd.subscriber_nodes) {
      subscriber_nodes.push_back(fbb.CreateString(node));
    }

    const auto topic_name = fbb.CreateString(cmd.topic_name);
    const auto message_type = fbb.CreateString(cmd.message_type);
    const auto publisher_nodes_offset = fbb.CreateVector(publisher_nodes);
    const auto subscriber_nodes_offset = fbb.CreateVector(subscriber_nodes);

    fbs::TopicInfoResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_topic_name(topic_name);
    builder.add_message_type(message_type);
    builder.add_publisher_count(cmd.publisher_count);
    builder.add_subscriber_count(cmd.subscriber_count);
    builder.add_publisher_nodes(publisher_nodes_offset);
    builder.add_subscriber_nodes(subscriber_nodes_offset);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::TopicInfoResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

inline void TopicSubscribeHandler::Receive(
    CommandQueue& in, const fbs::BridgePacket& pkt,
    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  // Use the default PMR resource — strings are enqueued and outlive the
  // per-datagram arena which is reset immediately after dispatch returns.
  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_TopicSubscribePacket();

  TopicSubscribeCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.topic_name = std::pmr::string(inner->topic_name()->string_view(), mr);
  cmd.message_type = std::pmr::string(inner->message_type()->string_view(), mr);
  cmd.once = inner->once();
  cmd.timeout_seconds = inner->timeout_seconds();
  cmd.raw = inner->raw();
  in.Enqueue(in_producer, std::move(cmd));
}

inline void TopicPublishMessageHandler::Receive(
    CommandQueue& in, const fbs::BridgePacket& pkt,
    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_TopicPublishMessagePacket();

  TopicPublishMessageCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.topic_name = std::pmr::string(inner->topic_name()->string_view(), mr);
  cmd.message_type = std::pmr::string(inner->message_type()->string_view(), mr);
  if (const auto* payload = inner->payload()) {
    cmd.payload.assign(payload->data(), payload->data() + payload->size());
  }
  cmd.once = inner->once();
  cmd.rate_hz = inner->rate_hz();
  cmd.times = inner->times();
  if (const auto* qos = inner->qos_profile()) {
    cmd.qos_profile = std::pmr::string(qos->string_view(), mr);
  }
  in.Enqueue(in_producer, std::move(cmd));
}

inline void TopicHzHandler::Receive(CommandQueue& in,
                                    const fbs::BridgePacket& pkt,
                                    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_TopicHzPacket();

  TopicHzCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.topic_name = std::pmr::string(inner->topic_name()->string_view(), mr);
  cmd.message_type = std::pmr::string(inner->message_type()->string_view(), mr);
  cmd.window = inner->window();
  cmd.wall_time = inner->wall_time();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void TopicHzHandler::DrainAndFlush(CommandQueue& out, Sink& sink,
                                          flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<TopicHzResponseCmd>();

  TopicHzResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto topic_name = fbb.CreateString(cmd.topic_name);

    fbs::TopicHzResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_topic_name(topic_name);
    builder.add_frequency(cmd.frequency);
    builder.add_window(cmd.window);
    builder.add_message_count(cmd.message_count);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::TopicHzResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

inline void TopicBwHandler::Receive(CommandQueue& in,
                                    const fbs::BridgePacket& pkt,
                                    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_TopicBwPacket();

  TopicBwCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.topic_name = std::pmr::string(inner->topic_name()->string_view(), mr);
  cmd.message_type = std::pmr::string(inner->message_type()->string_view(), mr);
  cmd.window = inner->window();
  cmd.wall_time = inner->wall_time();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void TopicBwHandler::DrainAndFlush(CommandQueue& out, Sink& sink,
                                          flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<TopicBwResponseCmd>();

  TopicBwResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto topic_name = fbb.CreateString(cmd.topic_name);

    fbs::TopicBwResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_topic_name(topic_name);
    builder.add_bytes_per_second(cmd.bytes_per_second);
    builder.add_window(cmd.window);
    builder.add_message_count(cmd.message_count);
    builder.add_total_bytes(cmd.total_bytes);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::TopicBwResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

inline void TopicDelayHandler::Receive(CommandQueue& in,
                                       const fbs::BridgePacket& pkt,
                                       std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_TopicDelayPacket();

  TopicDelayCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.topic_name = std::pmr::string(inner->topic_name()->string_view(), mr);
  if (const auto* message_type = inner->message_type()) {
    cmd.message_type = std::pmr::string(message_type->string_view(), mr);
  }
  cmd.window = inner->window();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void TopicDelayHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<TopicDelayResponseCmd>();

  TopicDelayResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto topic_name = fbb.CreateString(cmd.topic_name);

    fbs::TopicDelayResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_topic_name(topic_name);
    builder.add_average_delay(cmd.average_delay);
    builder.add_min_delay(cmd.min_delay);
    builder.add_max_delay(cmd.max_delay);
    builder.add_window(cmd.window);
    builder.add_message_count(cmd.message_count);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::TopicDelayResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

template <PacketSink Sink>
inline void TopicPayloadHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<TopicPayloadCmd>();

  TopicPayloadCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto topic_name = fbb.CreateString(cmd.topic_name);
    const auto message_type = fbb.CreateString(cmd.message_type);
    const auto payload =
        fbb.CreateVector(cmd.payload.data(), cmd.payload.size());

    fbs::TopicPayloadPacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_topic_name(topic_name);
    builder.add_message_type(message_type);
    builder.add_raw(cmd.raw);
    builder.add_payload(payload);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::TopicPayloadPacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

}  // namespace roscraft::bridge
