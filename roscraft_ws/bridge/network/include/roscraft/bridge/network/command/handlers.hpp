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

struct NodeInfoHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::NodeInfoPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  void DrainAndSend(CommandQueue& out, UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static NodeInfoHandler From(CommandQueue& in,
                                            CommandQueue& out) {
    return {in.MakeProducerToken<NodeInfoCmd>(),
            out.MakeConsumerToken<NodeInfoResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

struct TopicInfoHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::TopicInfoPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  void DrainAndSend(CommandQueue& out, UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static TopicInfoHandler From(CommandQueue& in,
                                             CommandQueue& out) {
    return {in.MakeProducerToken<TopicInfoCmd>(),
            out.MakeConsumerToken<TopicInfoResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

struct ServiceInfoHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::ServiceInfoPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  void DrainAndSend(CommandQueue& out, UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ServiceInfoHandler From(CommandQueue& in,
                                               CommandQueue& out) {
    return {in.MakeProducerToken<ServiceInfoCmd>(),
            out.MakeConsumerToken<ServiceInfoResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

struct InterfaceListHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::InterfaceListPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  void DrainAndSend(CommandQueue& out, UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static InterfaceListHandler From(CommandQueue& in,
                                                 CommandQueue& out) {
    return {in.MakeProducerToken<InterfaceListCmd>(),
            out.MakeConsumerToken<InterfaceListResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

struct InterfaceShowHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::InterfaceShowPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  void DrainAndSend(CommandQueue& out, UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static InterfaceShowHandler From(CommandQueue& in,
                                                 CommandQueue& out) {
    return {in.MakeProducerToken<InterfaceShowCmd>(),
            out.MakeConsumerToken<InterfaceShowResponseCmd>()};
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

    SendFbb(transport, fbb);
  }
}

inline void NodeInfoHandler::Receive(CommandQueue& in,
                                     const fbs::BridgePacket& pkt,
                                     std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  const auto* inner = pkt.payload_as_NodeInfoPacket();
  auto* mr = std::pmr::get_default_resource();

  NodeInfoCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.node_name = std::pmr::string(inner->node_name()->string_view(), mr);
  cmd.include_hidden = inner->include_hidden();
  in.Enqueue(in_producer, std::move(cmd));
}

inline void NodeInfoHandler::DrainAndSend(CommandQueue& out,
                                          UdpTransport& transport,
                                          flatbuffers::FlatBufferBuilder& fbb) {
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

    for (const auto& publisher : cmd.publishers) {
      publishers.push_back(
          fbs::CreateTopicEntry(fbb, fbb.CreateString(publisher.name),
                                fbb.CreateString(publisher.type)));
    }

    for (const auto& subscriber : cmd.subscribers) {
      subscribers.push_back(
          fbs::CreateTopicEntry(fbb, fbb.CreateString(subscriber.name),
                                fbb.CreateString(subscriber.type)));
    }

    for (const auto& service : cmd.services) {
      services.push_back(fbs::CreateServiceEntry(
          fbb, fbb.CreateString(service.name), fbb.CreateString(service.type)));
    }

    const auto node_name_offset = fbb.CreateString(cmd.node_name);
    const auto publishers_offset = fbb.CreateVector(publishers);
    const auto subscribers_offset = fbb.CreateVector(subscribers);
    const auto services_offset = fbb.CreateVector(services);

    fbs::NodeInfoResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_node_name(node_name_offset);
    builder.add_publishers(publishers_offset);
    builder.add_subscribers(subscribers_offset);
    builder.add_services(services_offset);
    builder.add_found(cmd.found);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::NodeInfoResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    SendFbb(transport, fbb);
  }
}

inline void TopicInfoHandler::Receive(CommandQueue& in,
                                      const fbs::BridgePacket& pkt,
                                      std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  const auto* inner = pkt.payload_as_TopicInfoPacket();
  auto* mr = std::pmr::get_default_resource();

  TopicInfoCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.topic_name = std::pmr::string(inner->topic_name()->string_view(), mr);
  in.Enqueue(in_producer, std::move(cmd));
}

inline void TopicInfoHandler::DrainAndSend(
    CommandQueue& out, UdpTransport& transport,
    flatbuffers::FlatBufferBuilder& fbb) {
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

    for (const auto& publisher_node : cmd.publisher_nodes) {
      publisher_nodes.push_back(fbb.CreateString(publisher_node));
    }

    for (const auto& subscriber_node : cmd.subscriber_nodes) {
      subscriber_nodes.push_back(fbb.CreateString(subscriber_node));
    }

    const auto topic_name_offset = fbb.CreateString(cmd.topic_name);
    const auto message_type_offset = fbb.CreateString(cmd.message_type);
    const auto publisher_nodes_offset = fbb.CreateVector(publisher_nodes);
    const auto subscriber_nodes_offset = fbb.CreateVector(subscriber_nodes);

    fbs::TopicInfoResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_topic_name(topic_name_offset);
    builder.add_message_type(message_type_offset);
    builder.add_publisher_count(cmd.publisher_count);
    builder.add_subscriber_count(cmd.subscriber_count);
    builder.add_publisher_nodes(publisher_nodes_offset);
    builder.add_subscriber_nodes(subscriber_nodes_offset);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::TopicInfoResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    SendFbb(transport, fbb);
  }
}

inline void ServiceInfoHandler::Receive(CommandQueue& in,
                                        const fbs::BridgePacket& pkt,
                                        std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  const auto* inner = pkt.payload_as_ServiceInfoPacket();
  auto* mr = std::pmr::get_default_resource();

  ServiceInfoCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.service_name = std::pmr::string(inner->service_name()->string_view(), mr);
  in.Enqueue(in_producer, std::move(cmd));
}

inline void ServiceInfoHandler::DrainAndSend(
    CommandQueue& out, UdpTransport& transport,
    flatbuffers::FlatBufferBuilder& fbb) {
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

    for (const auto& client_node : cmd.client_nodes) {
      client_nodes.push_back(fbb.CreateString(client_node));
    }

    for (const auto& server_node : cmd.server_nodes) {
      server_nodes.push_back(fbb.CreateString(server_node));
    }

    const auto service_name_offset = fbb.CreateString(cmd.service_name);
    const auto service_type_offset = fbb.CreateString(cmd.service_type);
    const auto client_nodes_offset = fbb.CreateVector(client_nodes);
    const auto server_nodes_offset = fbb.CreateVector(server_nodes);

    fbs::ServiceInfoResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_service_name(service_name_offset);
    builder.add_service_type(service_type_offset);
    builder.add_client_count(cmd.client_count);
    builder.add_server_count(cmd.server_count);
    builder.add_client_nodes(client_nodes_offset);
    builder.add_server_nodes(server_nodes_offset);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ServiceInfoResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    SendFbb(transport, fbb);
  }
}

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

inline void InterfaceListHandler::DrainAndSend(
    CommandQueue& out, UdpTransport& transport,
    flatbuffers::FlatBufferBuilder& fbb) {
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

    for (const auto& message : cmd.messages) {
      messages.push_back(fbb.CreateString(message));
    }

    for (const auto& service : cmd.services) {
      services.push_back(fbb.CreateString(service));
    }

    for (const auto& action : cmd.actions) {
      actions.push_back(fbb.CreateString(action));
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

    SendFbb(transport, fbb);
  }
}

inline void InterfaceShowHandler::Receive(
    CommandQueue& in, const fbs::BridgePacket& pkt,
    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  const auto* inner = pkt.payload_as_InterfaceShowPacket();
  auto* mr = std::pmr::get_default_resource();

  InterfaceShowCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.interface_type =
      std::pmr::string(inner->interface_type()->string_view(), mr);
  in.Enqueue(in_producer, std::move(cmd));
}

inline void InterfaceShowHandler::DrainAndSend(
    CommandQueue& out, UdpTransport& transport,
    flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<InterfaceShowResponseCmd>();

  InterfaceShowResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto interface_type_offset = fbb.CreateString(cmd.interface_type);
    const auto definition_offset = fbb.CreateString(cmd.definition);

    fbs::InterfaceShowResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_interface_type(interface_type_offset);
    builder.add_definition(definition_offset);
    builder.add_found(cmd.found);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::InterfaceShowResponsePacket, inner.Union());
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
  cmd.once = inner->once();
  cmd.timeout_seconds = inner->timeout_seconds();
  cmd.raw = inner->raw();
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

struct TopicHzHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::TopicHzPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  void DrainAndSend(CommandQueue& out, UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static TopicHzHandler From(CommandQueue& in,
                                           CommandQueue& out) {
    return {in.MakeProducerToken<TopicHzCmd>(),
            out.MakeConsumerToken<TopicHzResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

struct TopicBwHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::TopicBwPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  void DrainAndSend(CommandQueue& out, UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static TopicBwHandler From(CommandQueue& in,
                                           CommandQueue& out) {
    return {in.MakeProducerToken<TopicBwCmd>(),
            out.MakeConsumerToken<TopicBwResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
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

inline void TopicHzHandler::Receive(CommandQueue& in,
                                    const fbs::BridgePacket& pkt,
                                    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  const auto* inner = pkt.payload_as_TopicHzPacket();
  auto* mr = std::pmr::get_default_resource();

  TopicHzCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.topic_name = std::pmr::string(inner->topic_name()->string_view(), mr);
  cmd.message_type = std::pmr::string(inner->message_type()->string_view(), mr);
  cmd.window = inner->window();
  in.Enqueue(in_producer, std::move(cmd));
}

inline void TopicHzHandler::DrainAndSend(CommandQueue& out,
                                         UdpTransport& transport,
                                         flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<TopicHzResponseCmd>();

  TopicHzResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto topic_name_offset = fbb.CreateString(cmd.topic_name);

    fbs::TopicHzResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_topic_name(topic_name_offset);
    builder.add_frequency(cmd.frequency);
    builder.add_window(cmd.window);
    builder.add_message_count(cmd.message_count);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::TopicHzResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    SendFbb(transport, fbb);
  }
}

inline void TopicBwHandler::Receive(CommandQueue& in,
                                    const fbs::BridgePacket& pkt,
                                    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  const auto* inner = pkt.payload_as_TopicBwPacket();
  auto* mr = std::pmr::get_default_resource();

  TopicBwCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.topic_name = std::pmr::string(inner->topic_name()->string_view(), mr);
  cmd.message_type = std::pmr::string(inner->message_type()->string_view(), mr);
  cmd.window = inner->window();
  in.Enqueue(in_producer, std::move(cmd));
}

inline void TopicBwHandler::DrainAndSend(CommandQueue& out,
                                         UdpTransport& transport,
                                         flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<TopicBwResponseCmd>();

  TopicBwResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto topic_name_offset = fbb.CreateString(cmd.topic_name);

    fbs::TopicBwResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_topic_name(topic_name_offset);
    builder.add_bytes_per_second(cmd.bytes_per_second);
    builder.add_window(cmd.window);
    builder.add_message_count(cmd.message_count);
    builder.add_total_bytes(cmd.total_bytes);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::TopicBwResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    SendFbb(transport, fbb);
  }
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
    builder.add_request_id(cmd.request_id);
    builder.add_topic_name(topic_name_offset);
    builder.add_message_type(message_type_offset);
    builder.add_raw(cmd.raw);
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
    case fbs::PacketPayload::NodeInfoPacket:
      registry.Receive<NodeInfoHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::TopicInfoPacket:
      registry.Receive<TopicInfoHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::ServiceInfoPacket:
      registry.Receive<ServiceInfoHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::InterfaceListPacket:
      registry.Receive<InterfaceListHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::InterfaceShowPacket:
      registry.Receive<InterfaceShowHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::SubscribeTopicPacket:
      registry.Receive<SubscribeTopicHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::PublishMessagePacket:
      registry.Receive<PublishMessageHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::TopicHzPacket:
      registry.Receive<TopicHzHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::TopicBwPacket:
      registry.Receive<TopicBwHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::QueryPlayersPacket:
      registry.Receive<PlayerListHandler>(in, pkt, arena);
      return;
    default:
      return;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler: Error (send only — bridge error responses)
// ─────────────────────────────────────────────────────────────────────────────

struct ErrorHandler {
  void DrainAndSend(CommandQueue& out, UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ErrorHandler From(CommandQueue& out) {
    return {out.MakeConsumerToken<ErrorCmd>()};
  }

  CommandQueueConsumerToken out_consumer;
};

inline void ErrorHandler::DrainAndSend(CommandQueue& out,
                                       UdpTransport& transport,
                                       flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ErrorCmd>();

  ErrorCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto code_offset = fbb.CreateString(cmd.error_code);
    const auto msg_offset = fbb.CreateString(cmd.error_message);

    fbs::ErrorPacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_error_code(code_offset);
    builder.add_error_message(msg_offset);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ErrorPacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    SendFbb(transport, fbb);
  }
}

using DrainAndSendHandlerTypes =
    std::tuple<GraphHandler, NodeInfoHandler, TopicInfoHandler,
               ServiceInfoHandler, InterfaceListHandler, InterfaceShowHandler,
               TopicHzHandler, TopicBwHandler, PlayerListHandler,
               TopicPayloadHandler, ErrorHandler>;

}  // namespace network

}  // namespace roscraft::bridge
