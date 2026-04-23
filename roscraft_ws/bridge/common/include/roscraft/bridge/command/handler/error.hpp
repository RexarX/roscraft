#pragma once

#include <roscraft/bridge/command/handler.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/error.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

namespace roscraft::bridge {

// ─────────────────────────────────────────────────────────────────────────────
// ErrorHandler — Error (send only — bridge error responses)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Drains `ErrorCmd` and sends serialized error packets to clients.
struct ErrorHandler {
  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  [[nodiscard]] static ErrorHandler From(CommandQueue& out) {
    return {out.MakeConsumerToken<ErrorCmd>()};
  }

  CommandQueueConsumerToken out_consumer;
};

template <PacketSink Sink>
inline void ErrorHandler::DrainAndFlush(CommandQueue& out, Sink& sink,
                                        flatbuffers::FlatBufferBuilder& fbb) {
  auto& storage = out.TypedStorage<ErrorCmd>();

  ErrorCmd cmd;
  while (storage.Dequeue(out_consumer, cmd)) {
    fbb.Clear();

    const auto error_code = fbb.CreateString(cmd.error_code);
    const auto error_message = fbb.CreateString(cmd.error_message);

    fbs::ErrorPacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_error_code(error_code);
    builder.add_error_message(error_message);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::ErrorPacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

}  // namespace roscraft::bridge
