#include <doctest/doctest.h>

#include <roscraft/bridge/command/handler/graph.hpp>

#include "test_utils.hpp"

#include <memory_resource>

using namespace roscraft::bridge;

namespace {

[[nodiscard]] auto BuildQueryGraphPacket(uint64_t request_id)
    -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::QueryGraphPacket,
                            [request_id](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateQueryGraphPacket(fbb,
                                                                 request_id);
                            });
}

}  // namespace

TEST_SUITE("bridge::graph handlers") {
  TEST_CASE("bridge::GraphHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<QueryGraphCmd>();
    out.Register<GraphSnapshotCmd>();

    auto handler = GraphHandler::From(in, out);

    const auto bytes = BuildQueryGraphPacket(1001U);
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    QueryGraphCmd cmd{};
    CHECK(in.TypedStorage<QueryGraphCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 1001U);
  }

  TEST_CASE("bridge::GraphHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<QueryGraphCmd>();
    out.Register<GraphSnapshotCmd>();

    auto handler = GraphHandler::From(in, out);

    GraphSnapshotCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 1002U;
    {
      auto& node = cmd.nodes.emplace_back(std::pmr::get_default_resource());
      node.name = "/node";
    }
    {
      auto& topic = cmd.topics.emplace_back(std::pmr::get_default_resource());
      topic.name = "/topic";
      topic.type = "std_msgs/msg/String";
    }
    {
      auto& service =
          cmd.services.emplace_back(std::pmr::get_default_resource());
      service.name = "/service";
      service.type = "std_srvs/srv/Trigger";
    }
    {
      auto& action = cmd.actions.emplace_back(std::pmr::get_default_resource());
      action.name = "/action";
      action.type = "example_interfaces/action/Fibonacci";
    }
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::GraphSnapshotPacket);

    const auto* snapshot = packet->payload_as_GraphSnapshotPacket();
    REQUIRE(snapshot != nullptr);
    CHECK_EQ(snapshot->request_id(), 1002U);
    REQUIRE(snapshot->nodes() != nullptr);
    REQUIRE(snapshot->topics() != nullptr);
    REQUIRE(snapshot->services() != nullptr);
    REQUIRE(snapshot->actions() != nullptr);
    CHECK_EQ(snapshot->nodes()->size(), 1U);
    CHECK_EQ(snapshot->topics()->size(), 1U);
    CHECK_EQ(snapshot->services()->size(), 1U);
    CHECK_EQ(snapshot->actions()->size(), 1U);
    CHECK_EQ(snapshot->nodes()->Get(0)->name()->str(), "/node");
    CHECK_EQ(snapshot->topics()->Get(0)->name()->str(), "/topic");
    CHECK_EQ(snapshot->topics()->Get(0)->type()->str(), "std_msgs/msg/String");
    CHECK_EQ(snapshot->services()->Get(0)->name()->str(), "/service");
    CHECK_EQ(snapshot->services()->Get(0)->type()->str(),
             "std_srvs/srv/Trigger");
    CHECK_EQ(snapshot->actions()->Get(0)->name()->str(), "/action");
    CHECK_EQ(snapshot->actions()->Get(0)->type()->str(),
             "example_interfaces/action/Fibonacci");
    CHECK_EQ(out.CommandCount<GraphSnapshotCmd>(), 0U);
  }
}
