#include <doctest/doctest.h>

#include <roscraft/bridge/command/handler/service.hpp>

#include "test_utils.hpp"

#include <array>
#include <memory_resource>

using namespace roscraft::bridge;

namespace {

[[nodiscard]] auto BuildServiceInfoPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::ServiceInfoPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateServiceInfoPacketDirect(
                                  fbb, 111U, "/service");
                            });
}

[[nodiscard]] auto BuildServiceCallPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(
      fbs::PacketPayload::ServiceCallPacket,
      [](flatbuffers::FlatBufferBuilder& fbb) {
        const auto service_name = fbb.CreateString("/service");
        const auto service_type = fbb.CreateString("std_srvs/srv/Trigger");
        const std::array<uint8_t, 3> payload{1U, 2U, 3U};
        const auto payload_offset =
            fbb.CreateVector(payload.data(), payload.size());
        return fbs::CreateServiceCallPacket(fbb, 222U, service_name,
                                            service_type, payload_offset, 0.5,
                                            2U, 10.0);
      });
}

}  // namespace

TEST_SUITE("bridge::service handlers") {
  TEST_CASE("bridge::ServiceInfoHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ServiceInfoCmd>();
    out.Register<ServiceInfoResponseCmd>();

    auto handler = ServiceInfoHandler::From(in, out);

    const auto bytes = BuildServiceInfoPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    ServiceInfoCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<ServiceInfoCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 111U);
    CHECK_EQ(cmd.service_name, "/service");
  }

  TEST_CASE("bridge::ServiceInfoHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ServiceInfoCmd>();
    out.Register<ServiceInfoResponseCmd>();

    auto handler = ServiceInfoHandler::From(in, out);

    ServiceInfoResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 333U;
    cmd.service_name = "/service";
    cmd.service_type = "std_srvs/srv/Trigger";
    cmd.client_count = 1U;
    cmd.server_count = 1U;
    cmd.client_nodes.emplace_back("/client");
    cmd.server_nodes.emplace_back("/server");
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::ServiceInfoResponsePacket);

    const auto* response = packet->payload_as_ServiceInfoResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 333U);
    CHECK_EQ(response->service_name()->str(), "/service");
    CHECK_EQ(response->service_type()->str(), "std_srvs/srv/Trigger");
    CHECK_EQ(response->client_count(), 1U);
    CHECK_EQ(response->server_count(), 1U);
    REQUIRE(response->client_nodes() != nullptr);
    REQUIRE(response->server_nodes() != nullptr);
    CHECK_EQ(response->client_nodes()->Get(0)->str(), "/client");
    CHECK_EQ(response->server_nodes()->Get(0)->str(), "/server");
    CHECK_EQ(out.CommandCount<ServiceInfoResponseCmd>(), 0U);
  }

  TEST_CASE("bridge::ServiceCallHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ServiceCallCmd>();
    out.Register<ServiceCallResponseCmd>();

    auto handler = ServiceCallHandler::From(in, out);

    const auto bytes = BuildServiceCallPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    ServiceCallCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<ServiceCallCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 222U);
    CHECK_EQ(cmd.service_name, "/service");
    CHECK_EQ(cmd.service_type, "std_srvs/srv/Trigger");
    CHECK_EQ(cmd.payload.size(), 3U);
    CHECK_EQ(cmd.payload[0], 1U);
    CHECK_EQ(cmd.payload[2], 3U);
    CHECK_EQ(cmd.timeout_seconds, doctest::Approx(0.5));
    CHECK_EQ(cmd.repeat_count, 2U);
    CHECK_EQ(cmd.rate_hz, doctest::Approx(10.0));
  }

  TEST_CASE("bridge::ServiceCallHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ServiceCallCmd>();
    out.Register<ServiceCallResponseCmd>();

    auto handler = ServiceCallHandler::From(in, out);

    ServiceCallResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 444U;
    cmd.service_name = "/service";
    cmd.service_type = "std_srvs/srv/Trigger";
    cmd.success = true;
    cmd.response_payload = {9U, 8U};
    cmd.result_text = "ok";
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::ServiceCallResponsePacket);

    const auto* response = packet->payload_as_ServiceCallResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 444U);
    CHECK_EQ(response->service_name()->str(), "/service");
    CHECK_EQ(response->service_type()->str(), "std_srvs/srv/Trigger");
    CHECK(response->success());
    REQUIRE(response->response_payload() != nullptr);
    CHECK_EQ(response->response_payload()->size(), 2U);
    CHECK_EQ(response->response_payload()->Get(0), 9U);
    CHECK_EQ(response->response_payload()->Get(1), 8U);
    CHECK_EQ(response->result_text()->str(), "ok");
    CHECK_EQ(out.CommandCount<ServiceCallResponseCmd>(), 0U);
  }
}
