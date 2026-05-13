#include <doctest/doctest.h>

#include <roscraft/bridge/command/handler/addon.hpp>

#include "test_utils.hpp"

#include <memory_resource>

using namespace roscraft::bridge;

namespace {

[[nodiscard]] auto BuildAddonEventPacket(uint64_t request_id,
                                         std::string_view addon_id,
                                         std::string_view event_type)
    -> std::vector<uint8_t> {
  return tests::BuildPacket(
      fbs::PacketPayload::AddonEventPacket,
      [=](flatbuffers::FlatBufferBuilder& fbb) {
        const auto addon_id_off = fbb.CreateString(addon_id);
        const auto event_type_off = fbb.CreateString(event_type);
        return fbs::CreateAddonEventPacket(fbb, request_id, addon_id_off,
                                           event_type_off);
      });
}

}  // namespace

TEST_SUITE("bridge::addon handlers") {
  TEST_CASE("bridge::AddonEventHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<AddonEventCmd>();
    out.Register<AddonEventCmd>();

    auto handler = AddonEventHandler::From(in, out);

    const auto bytes = BuildAddonEventPacket(100U, "ping", "hello");
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    AddonEventCmd cmd{};
    CHECK(in.TypedStorage<AddonEventCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 100U);
    CHECK_EQ(cmd.addon_id, "ping");
    CHECK_EQ(cmd.event_type, "hello");
    CHECK_FALSE(cmd.response);
  }

  TEST_CASE("bridge::AddonEventHandler::Receive with payload") {
    CommandQueue in;
    CommandQueue out;
    in.Register<AddonEventCmd>();
    out.Register<AddonEventCmd>();

    auto handler = AddonEventHandler::From(in, out);

    const auto bytes = tests::BuildPacket(
        fbs::PacketPayload::AddonEventPacket,
        [](flatbuffers::FlatBufferBuilder& fbb) {
          const auto addon_id_off = fbb.CreateString("echo");
          const auto event_type_off = fbb.CreateString("say");
          const auto encoding_off = fbb.CreateString("json");
          const std::vector<uint8_t> payload_data = {0x7B, 0x7D};
          const auto payload_off = fbb.CreateVector(payload_data);
          return fbs::CreateAddonEventPacket(fbb, 200U, addon_id_off,
                                             event_type_off, encoding_off,
                                             payload_off, true);
        });
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    AddonEventCmd cmd{};
    CHECK(in.TypedStorage<AddonEventCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 200U);
    CHECK_EQ(cmd.addon_id, "echo");
    CHECK_EQ(cmd.event_type, "say");
    CHECK_EQ(cmd.encoding, "json");
    CHECK(cmd.response);
    CHECK_EQ(cmd.payload.size(), 2U);
    CHECK_EQ(cmd.payload[0], 0x7B);
    CHECK_EQ(cmd.payload[1], 0x7D);
  }

  TEST_CASE("bridge::AddonEventHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<AddonEventCmd>();
    out.Register<AddonEventCmd>();

    auto handler = AddonEventHandler::From(in, out);

    AddonEventCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 300U;
    cmd.addon_id = "ping";
    cmd.event_type = "pong";
    cmd.encoding = "json";
    cmd.response = true;
    cmd.payload = {0x01};
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::AddonEventPacket);

    const auto* response = packet->payload_as_AddonEventPacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 300U);
    CHECK_EQ(response->addon_id()->str(), "ping");
    CHECK_EQ(response->event_type()->str(), "pong");
    CHECK_EQ(response->encoding()->str(), "json");
    CHECK(response->response());
    CHECK_EQ(response->payload()->size(), 1U);
    CHECK_EQ(response->payload()->Get(0), 0x01);
    CHECK_EQ(out.CommandCount<AddonEventCmd>(), 0U);
  }

  TEST_CASE("bridge::AddonEventHandler::DrainAndFlush empty queue") {
    CommandQueue in;
    CommandQueue out;
    in.Register<AddonEventCmd>();
    out.Register<AddonEventCmd>();

    auto handler = AddonEventHandler::From(in, out);

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    CHECK(sink.packets.empty());
  }
}
