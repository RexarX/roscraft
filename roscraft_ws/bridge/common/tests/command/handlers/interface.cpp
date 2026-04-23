#include <doctest/doctest.h>

#include <roscraft/bridge/command/handler/interface.hpp>

#include "test_utils.hpp"

#include <memory_resource>

using namespace roscraft::bridge;

namespace {

[[nodiscard]] auto BuildInterfaceListPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::InterfaceListPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateInterfaceListPacket(
                                  fbb, 11U, true, false, true);
                            });
}

[[nodiscard]] auto BuildInterfaceShowPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::InterfaceShowPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateInterfaceShowPacketDirect(
                                  fbb, 22U, "std_msgs/msg/String");
                            });
}

}  // namespace

TEST_SUITE("bridge::interface handlers") {
  TEST_CASE("bridge::InterfaceListHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<InterfaceListCmd>();
    out.Register<InterfaceListResponseCmd>();

    auto handler = InterfaceListHandler::From(in, out);

    const auto bytes = BuildInterfaceListPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    InterfaceListCmd cmd{};
    CHECK(in.TypedStorage<InterfaceListCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 11U);
    CHECK(cmd.include_messages);
    CHECK_FALSE(cmd.include_services);
    CHECK(cmd.include_actions);
  }

  TEST_CASE("bridge::InterfaceListHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<InterfaceListCmd>();
    out.Register<InterfaceListResponseCmd>();

    auto handler = InterfaceListHandler::From(in, out);

    InterfaceListResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 33U;
    cmd.messages.emplace_back("std_msgs/msg/String");
    cmd.services.emplace_back("std_srvs/srv/Trigger");
    cmd.actions.emplace_back("example_interfaces/action/Fibonacci");
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::InterfaceListResponsePacket);

    const auto* response = packet->payload_as_InterfaceListResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 33U);
    REQUIRE(response->messages() != nullptr);
    REQUIRE(response->services() != nullptr);
    REQUIRE(response->actions() != nullptr);
    CHECK_EQ(response->messages()->size(), 1U);
    CHECK_EQ(response->services()->size(), 1U);
    CHECK_EQ(response->actions()->size(), 1U);
    CHECK_EQ(response->messages()->Get(0)->str(), "std_msgs/msg/String");
    CHECK_EQ(response->services()->Get(0)->str(), "std_srvs/srv/Trigger");
    CHECK_EQ(response->actions()->Get(0)->str(),
             "example_interfaces/action/Fibonacci");
    CHECK_EQ(out.CommandCount<InterfaceListResponseCmd>(), 0U);
  }

  TEST_CASE("bridge::InterfaceShowHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<InterfaceShowCmd>();
    out.Register<InterfaceShowResponseCmd>();

    auto handler = InterfaceShowHandler::From(in, out);

    const auto bytes = BuildInterfaceShowPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    InterfaceShowCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<InterfaceShowCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 22U);
    CHECK_EQ(cmd.interface_type, "std_msgs/msg/String");
  }

  TEST_CASE("bridge::InterfaceShowHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<InterfaceShowCmd>();
    out.Register<InterfaceShowResponseCmd>();

    auto handler = InterfaceShowHandler::From(in, out);

    InterfaceShowResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 44U;
    cmd.interface_type = "std_msgs/msg/String";
    cmd.definition = "string data";
    cmd.found = true;
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::InterfaceShowResponsePacket);

    const auto* response = packet->payload_as_InterfaceShowResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 44U);
    CHECK_EQ(response->interface_type()->str(), "std_msgs/msg/String");
    CHECK_EQ(response->definition()->str(), "string data");
    CHECK(response->found());
    CHECK_EQ(out.CommandCount<InterfaceShowResponseCmd>(), 0U);
  }
}
