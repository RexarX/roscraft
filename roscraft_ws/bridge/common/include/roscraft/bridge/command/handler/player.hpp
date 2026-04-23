#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/handler.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/player.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

#include <memory_resource>
#include <vector>

namespace roscraft::bridge {

// ─────────────────────────────────────────────────────────────────────────────
// PlayerListHandler — QueryPlayers (receive) / PlayerList (send)
// ─────────────────────────────────────────────────────────────────────────────

/// @brief Handles `QueryPlayersPacket` (receive) and `PlayerListCmd` (drain).
struct PlayerListHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::QueryPlayersPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
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

template <PacketSink Sink>
inline void PlayerListHandler::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
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

    const auto players = fbb.CreateVector(entries);

    fbs::PlayerListPacketBuilder builder(fbb);
    builder.add_request_id(cmd.request_id);
    builder.add_players(players);
    const auto inner = builder.Finish();
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::PlayerListPacket, inner.Union());
    fbs::FinishBridgePacketBuffer(fbb, root);

    sink.Send(fbb);
  }
}

}  // namespace roscraft::bridge
