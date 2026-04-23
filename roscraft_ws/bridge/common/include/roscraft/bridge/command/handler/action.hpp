#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/handler.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/action.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

#include <memory_resource>
#include <string>

namespace roscraft::bridge {

// ─────────────────────────────────────────────────────────────────────────────
// ActionInfoHandler — ActionInfo (receive) / ActionInfoResponse (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `ActionInfoPacket` (receive) and `ActionInfoResponseCmd`
/// (drain).
struct ActionInfoHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::ActionInfoPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ActionInfoHandler From(CommandQueue& in,
                                              CommandQueue& out) {
    return {in.MakeProducerToken<ActionInfoCmd>(),
            out.MakeConsumerToken<ActionInfoResponseCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// ActionSendGoalHandler — ActionSendGoal (receive) / ActionResult (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `ActionSendGoalPacket` (receive) and `ActionResultCmd`
/// (drain).
struct ActionSendGoalHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::ActionSendGoalPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ActionSendGoalHandler From(CommandQueue& in,
                                                  CommandQueue& out) {
    return {in.MakeProducerToken<ActionSendGoalCmd>(),
            out.MakeConsumerToken<ActionResultCmd>()};
  }

  CommandQueueProducerToken in_producer;
  CommandQueueConsumerToken out_consumer;
};

// ─────────────────────────────────────────────────────────────────────────────
// ActionFeedbackHandler — ActionFeedback (send only — async push)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Drains `ActionFeedbackCmd` and sends serialized feedback to clients.
struct ActionFeedbackHandler {
  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ActionFeedbackHandler From(CommandQueue& out) {
    return {out.MakeConsumerToken<ActionFeedbackCmd>()};
  }

  CommandQueueConsumerToken out_consumer;
};

inline void ActionInfoHandler::Receive(CommandQueue& in,
                                       const fbs::BridgePacket& pkt,
                                       std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_ActionInfoPacket();

  ActionInfoCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.action_name = std::pmr::string(inner->action_name()->string_view(), mr);
  cmd.include_hidden = inner->include_hidden();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void ActionInfoHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ActionInfoResponseCmd>();

  ActionInfoResponseCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto action_name = fbb.CreateString(cmd.action_name);
    const auto action_type = fbb.CreateString(cmd.action_type);

    fbs::ActionInfoResponsePacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_action_name(action_name);
    builder.add_action_type(action_type);
    builder.add_client_count(cmd.client_count);
    builder.add_server_count(cmd.server_count);
    builder.add_feedback_publisher_count(cmd.feedback_publisher_count);
    builder.add_feedback_subscriber_count(cmd.feedback_subscriber_count);
    builder.add_status_publisher_count(cmd.status_publisher_count);
    builder.add_status_subscriber_count(cmd.status_subscriber_count);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ActionInfoResponsePacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

inline void ActionSendGoalHandler::Receive(
    CommandQueue& in, const fbs::BridgePacket& pkt,
    std::pmr::memory_resource& /*arena*/) {
  ROSCRAFT_ASSERT(pkt.payload_type() == kReceiveType,
                  "Invalid payload type, got '{}', expected '{}'!",
                  fbs::EnumNamePacketPayload(pkt.payload_type()),
                  fbs::EnumNamePacketPayload(kReceiveType));

  auto* mr = std::pmr::get_default_resource();
  const auto* inner = pkt.payload_as_ActionSendGoalPacket();

  ActionSendGoalCmd cmd(mr);
  cmd.request_id = inner->request_id();
  cmd.action_name = std::pmr::string(inner->action_name()->string_view(), mr);
  cmd.action_type = std::pmr::string(inner->action_type()->string_view(), mr);
  if (const auto* goal = inner->goal_payload()) {
    cmd.goal_payload.assign(goal->data(), goal->data() + goal->size());
  }
  cmd.feedback = inner->feedback();
  cmd.timeout_seconds = inner->timeout_seconds();
  in.Enqueue(in_producer, std::move(cmd));
}

template <PacketSink Sink>
inline void ActionSendGoalHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ActionResultCmd>();

  ActionResultCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto action_name = fbb.CreateString(cmd.action_name);
    const auto action_type = fbb.CreateString(cmd.action_type);
    const auto result_payload =
        fbb.CreateVector(cmd.result_payload.data(), cmd.result_payload.size());
    const auto result_text = fbb.CreateString(cmd.result_text);

    fbs::ActionResultPacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_action_name(action_name);
    builder.add_action_type(action_type);
    builder.add_success(cmd.success);
    builder.add_result_payload(result_payload);
    builder.add_result_text(result_text);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ActionResultPacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

template <PacketSink Sink>
inline void ActionFeedbackHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ActionFeedbackCmd>();

  ActionFeedbackCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto action_name = fbb.CreateString(cmd.action_name);
    const auto action_type = fbb.CreateString(cmd.action_type);
    const auto feedback_payload = fbb.CreateVector(cmd.feedback_payload.data(),
                                                   cmd.feedback_payload.size());
    const auto feedback_text = fbb.CreateString(cmd.feedback_text);

    fbs::ActionFeedbackPacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_action_name(action_name);
    builder.add_action_type(action_type);
    builder.add_feedback_payload(feedback_payload);
    builder.add_feedback_text(feedback_text);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ActionFeedbackPacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

}  // namespace roscraft::bridge
