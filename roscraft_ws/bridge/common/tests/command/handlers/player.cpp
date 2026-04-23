#include <doctest/doctest.h>

#include <roscraft/bridge/command/handler/player.hpp>

#include "test_utils.hpp"

#include <memory_resource>

using namespace roscraft::bridge;

namespace {

[[nodiscard]] auto BuildQueryPlayersPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::QueryPlayersPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateQueryPlayersPacket(fbb, 901U);
                            });
}

}  // namespace

TEST_SUITE("bridge::player handlers") {
  TEST_CASE("bridge::PlayerListHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<QueryPlayersCmd>();
    out.Register<PlayerListCmd>();

    auto handler = PlayerListHandler::From(in, out);

    const auto bytes = BuildQueryPlayersPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    QueryPlayersCmd cmd{};
    CHECK(in.TypedStorage<QueryPlayersCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 901U);
  }

  TEST_CASE("bridge::PlayerListHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<QueryPlayersCmd>();
    out.Register<PlayerListCmd>();

    auto handler = PlayerListHandler::From(in, out);

    PlayerListCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 902U;
    {
      auto& player = cmd.players.emplace_back(std::pmr::get_default_resource());
      player.name = "Alice";
      player.x = 1.0F;
      player.y = 2.0F;
      player.z = 3.0F;
    }
    {
      auto& player = cmd.players.emplace_back(std::pmr::get_default_resource());
      player.name = "Bob";
      player.x = 4.0F;
      player.y = 5.0F;
      player.z = 6.0F;
    }
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::PlayerListPacket);

    const auto* response = packet->payload_as_PlayerListPacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 902U);
    REQUIRE(response->players() != nullptr);
    CHECK_EQ(response->players()->size(), 2U);
    CHECK_EQ(response->players()->Get(0)->name()->str(), "Alice");
    CHECK_EQ(response->players()->Get(0)->x(), doctest::Approx(1.0F));
    CHECK_EQ(response->players()->Get(0)->y(), doctest::Approx(2.0F));
    CHECK_EQ(response->players()->Get(0)->z(), doctest::Approx(3.0F));
    CHECK_EQ(response->players()->Get(1)->name()->str(), "Bob");
    CHECK_EQ(response->players()->Get(1)->x(), doctest::Approx(4.0F));
    CHECK_EQ(response->players()->Get(1)->y(), doctest::Approx(5.0F));
    CHECK_EQ(response->players()->Get(1)->z(), doctest::Approx(6.0F));
    CHECK_EQ(out.CommandCount<PlayerListCmd>(), 0U);
  }
}
