#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/handler.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/param.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

#include <memory_resource>
#include <string>
#include <vector>

namespace roscraft::bridge {

// ─────────────────────────────────────────────────────────────────────────────
// ParamListHandler
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `ParamListPacket` (receive) and `ParamListResponseCmd`
/// (drain).
struct ParamListHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::ParamListPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ParamListHandler From(CommandQueue& in,
                                             CommandQueue& out) {
    return {in.MakeProducerToken<ParamListCmd>(),
            out.MakeConsumerToken<ParamListResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// ParamGetHandler
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `ParamGetPacket` (receive) and `ParamGetResponseCmd` (drain).
struct ParamGetHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::ParamGetPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ParamGetHandler From(CommandQueue& in,
                                            CommandQueue& out) {
    return {in.MakeProducerToken<ParamGetCmd>(),
            out.MakeConsumerToken<ParamGetResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// ParamSetHandler
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `ParamSetPacket` (receive) and `ParamSetResponseCmd` (drain).
struct ParamSetHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::ParamSetPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ParamSetHandler From(CommandQueue& in,
                                            CommandQueue& out) {
    return {in.MakeProducerToken<ParamSetCmd>(),
            out.MakeConsumerToken<ParamSetResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// ParamDescribeHandler
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `ParamDescribePacket` (receive) and
/// `ParamDescribeResponseCmd` (drain).
struct ParamDescribeHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::ParamDescribePacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ParamDescribeHandler From(CommandQueue& in,
                                                 CommandQueue& out) {
    return {in.MakeProducerToken<ParamDescribeCmd>(),
            out.MakeConsumerToken<ParamDescribeResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// ParamDumpHandler
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `ParamDumpPacket` (receive) and `ParamDumpResponseCmd`
/// (drain).
struct ParamDumpHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::ParamDumpPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ParamDumpHandler From(CommandQueue& in,
                                             CommandQueue& out) {
    return {in.MakeProducerToken<ParamDumpCmd>(),
            out.MakeConsumerToken<ParamDumpResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// ParamLoadHandler
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `ParamLoadPacket` (receive) and `ParamLoadResponseCmd`
/// (drain).
struct ParamLoadHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::ParamLoadPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ParamLoadHandler From(CommandQueue& in,
                                             CommandQueue& out) {
    return {in.MakeProducerToken<ParamLoadCmd>(),
            out.MakeConsumerToken<ParamLoadResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

inline void ParamListHandler::Receive(CommandQueue& in,
                                      const fbs::BridgePacket& pkt,
                                      std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_ParamListPacket();

  ParamListCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.node_name = std::pmr::string(inner->node_name()->string_view(), mr);
  if (const auto* prefixes = inner->prefixes()) {
    cmd.prefixes.reserve(prefixes->size());
    for (const auto* value : *prefixes) {
      cmd.prefixes.emplace_back(value->string_view());
    }
  }
  cmd.depth = inner->depth();
  cmd.include_types = inner->include_types();
  if (const auto* filter_regex = inner->filter_regex()) {
    cmd.filter_regex = std::pmr::string(filter_regex->string_view(), mr);
  }
  cmd.timeout_seconds = inner->timeout_seconds();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void ParamListHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ParamListResponseCmd>();

  std::vector<flatbuffers::Offset<flatbuffers::String>> names;
  std::vector<flatbuffers::Offset<flatbuffers::String>> prefixes;
  std::vector<flatbuffers::Offset<flatbuffers::String>> types;

  ParamListResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    names.clear();
    prefixes.clear();
    types.clear();
    names.reserve(cmd.names.size());
    prefixes.reserve(cmd.prefixes.size());
    types.reserve(cmd.types.size());

    for (const auto& name : cmd.names) {
      names.push_back(fbb.CreateString(name));
    }
    for (const auto& prefix : cmd.prefixes) {
      prefixes.push_back(fbb.CreateString(prefix));
    }
    for (const auto& type : cmd.types) {
      types.push_back(fbb.CreateString(type));
    }

    const auto node_name = fbb.CreateString(cmd.node_name);
    const auto names_offset = fbb.CreateVector(names);
    const auto prefixes_offset = fbb.CreateVector(prefixes);
    const auto types_offset = fbb.CreateVector(types);

    fbs::ParamListResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_node_name(node_name);
    builder.add_names(names_offset);
    builder.add_prefixes(prefixes_offset);
    builder.add_types(types_offset);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ParamListResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

inline void ParamGetHandler::Receive(CommandQueue& in,
                                     const fbs::BridgePacket& pkt,
                                     std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_ParamGetPacket();

  ParamGetCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.node_name = std::pmr::string(inner->node_name()->string_view(), mr);
  cmd.param_name = std::pmr::string(inner->param_name()->string_view(), mr);
  cmd.hide_type = inner->hide_type();
  cmd.timeout_seconds = inner->timeout_seconds();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void ParamGetHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ParamGetResponseCmd>();

  ParamGetResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto node_name = fbb.CreateString(cmd.node_name);
    const auto param_name = fbb.CreateString(cmd.param_name);
    const auto param_type = fbb.CreateString(cmd.param_type);
    const auto value_text = fbb.CreateString(cmd.value_text);

    fbs::ParamGetResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_node_name(node_name);
    builder.add_param_name(param_name);
    builder.add_found(cmd.found);
    builder.add_param_type(param_type);
    builder.add_value_text(value_text);
    builder.add_type_hidden(cmd.type_hidden);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ParamGetResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

inline void ParamSetHandler::Receive(CommandQueue& in,
                                     const fbs::BridgePacket& pkt,
                                     std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_ParamSetPacket();

  ParamSetCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.node_name = std::pmr::string(inner->node_name()->string_view(), mr);
  cmd.param_name = std::pmr::string(inner->param_name()->string_view(), mr);
  cmd.value_text = std::pmr::string(inner->value_text()->string_view(), mr);
  cmd.timeout_seconds = inner->timeout_seconds();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void ParamSetHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ParamSetResponseCmd>();

  ParamSetResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto node_name = fbb.CreateString(cmd.node_name);
    const auto param_name = fbb.CreateString(cmd.param_name);
    const auto reason = fbb.CreateString(cmd.reason);
    const auto param_type = fbb.CreateString(cmd.param_type);
    const auto value_text = fbb.CreateString(cmd.value_text);

    fbs::ParamSetResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_node_name(node_name);
    builder.add_param_name(param_name);
    builder.add_success(cmd.success);
    builder.add_reason(reason);
    builder.add_param_type(param_type);
    builder.add_value_text(value_text);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ParamSetResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

inline void ParamDescribeHandler::Receive(
    CommandQueue& in, const fbs::BridgePacket& pkt,
    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_ParamDescribePacket();

  ParamDescribeCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.node_name = std::pmr::string(inner->node_name()->string_view(), mr);
  cmd.param_name = std::pmr::string(inner->param_name()->string_view(), mr);
  cmd.timeout_seconds = inner->timeout_seconds();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void ParamDescribeHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ParamDescribeResponseCmd>();

  ParamDescribeResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto node_name = fbb.CreateString(cmd.node_name);
    const auto param_name = fbb.CreateString(cmd.param_name);
    const auto param_type = fbb.CreateString(cmd.param_type);
    const auto description = fbb.CreateString(cmd.description);
    const auto constraints = fbb.CreateString(cmd.constraints);

    fbs::ParamDescribeResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_node_name(node_name);
    builder.add_param_name(param_name);
    builder.add_found(cmd.found);
    builder.add_param_type(param_type);
    builder.add_description(description);
    builder.add_read_only(cmd.read_only);
    builder.add_constraints(constraints);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ParamDescribeResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

inline void ParamDumpHandler::Receive(CommandQueue& in,
                                      const fbs::BridgePacket& pkt,
                                      std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_ParamDumpPacket();

  ParamDumpCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.node_name = std::pmr::string(inner->node_name()->string_view(), mr);
  if (const auto* prefixes = inner->prefixes()) {
    cmd.prefixes.reserve(prefixes->size());
    for (const auto* value : *prefixes) {
      cmd.prefixes.emplace_back(value->string_view());
    }
  }
  cmd.timeout_seconds = inner->timeout_seconds();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void ParamDumpHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ParamDumpResponseCmd>();

  ParamDumpResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto node_name = fbb.CreateString(cmd.node_name);
    const auto yaml_text = fbb.CreateString(cmd.yaml_text);

    fbs::ParamDumpResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_node_name(node_name);
    builder.add_yaml_text(yaml_text);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ParamDumpResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

inline void ParamLoadHandler::Receive(CommandQueue& in,
                                      const fbs::BridgePacket& pkt,
                                      std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_ParamLoadPacket();

  ParamLoadCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.node_name = std::pmr::string(inner->node_name()->string_view(), mr);
  cmd.yaml_text = std::pmr::string(inner->yaml_text()->string_view(), mr);
  cmd.timeout_seconds = inner->timeout_seconds();
  cmd.use_wildcard = inner->use_wildcard();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void ParamLoadHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ParamLoadResponseCmd>();

  ParamLoadResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto node_name = fbb.CreateString(cmd.node_name);
    const auto reason = fbb.CreateString(cmd.reason);

    fbs::ParamLoadResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_node_name(node_name);
    builder.add_success(cmd.success);
    builder.add_reason(reason);
    builder.add_params_loaded(cmd.params_loaded);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ParamLoadResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

}  // namespace roscraft::bridge
