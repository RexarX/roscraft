#include <doctest/doctest.h>

#include <roscraft/bridge/command/handler/error.hpp>

#include "test_utils.hpp"

#include <memory_resource>

using namespace roscraft::bridge;

TEST_SUITE("bridge::error handlers") {
  TEST_CASE("bridge::ErrorHandler::DrainAndFlush") {
    CommandQueue out;
    out.Register<ErrorCmd>();

    auto handler = ErrorHandler::From(out);

    ErrorCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 404U;
    cmd.error_code = "TEST_ERROR";
    cmd.error_message = "something failed";
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::ErrorPacket);

    const auto* error_packet = packet->payload_as_ErrorPacket();
    REQUIRE(error_packet != nullptr);
    CHECK_EQ(error_packet->request_id(), 404U);
    CHECK_EQ(error_packet->error_code()->str(), "TEST_ERROR");
    CHECK_EQ(error_packet->error_message()->str(), "something failed");
    CHECK_EQ(out.CommandCount<ErrorCmd>(), 0U);
  }
}
