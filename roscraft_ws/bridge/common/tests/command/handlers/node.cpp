#include <doctest/doctest.h>

#include <roscraft/bridge/command/handler/node.hpp>

#include "test_utils.hpp"

#include <memory_resource>

using namespace roscraft::bridge;

namespace {

[[nodiscard]] auto BuildNodeInfoPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::NodeInfoPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateNodeInfoPacketDirect(
                                  fbb, 71U, "/demo_node", true);
                            });
}

}  // namespace

TEST_SUITE("bridge::node handlers") {
  TEST_CASE("bridge::NodeInfoHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<NodeInfoCmd>();
    out.Register<NodeInfoResponseCmd>();

    auto handler = NodeInfoHandler::From(in, out);

    const auto bytes = BuildNodeInfoPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    NodeInfoCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<NodeInfoCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 71U);
    CHECK_EQ(cmd.node_name, "/demo_node");
    CHECK(cmd.include_hidden);
  }

  TEST_CASE("bridge::NodeInfoHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<NodeInfoCmd>();
    out.Register<NodeInfoResponseCmd>();

    auto handler = NodeInfoHandler::From(in, out);

    NodeInfoResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 72U;
    cmd.node_name = "/demo_node";
    {
      auto& pub = cmd.publishers.emplace_back(std::pmr::get_default_resource());
      pub.name = "/topic_pub";
      pub.type = "std_msgs/msg/String";
    }
    {
      auto& sub =
          cmd.subscribers.emplace_back(std::pmr::get_default_resource());
      sub.name = "/topic_sub";
      sub.type = "std_msgs/msg/Int32";
    }
    {
      auto& svc = cmd.services.emplace_back(std::pmr::get_default_resource());
      svc.name = "/service";
      svc.type = "std_srvs/srv/Trigger";
    }
    cmd.found = true;
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::NodeInfoResponsePacket);

    const auto* response = packet->payload_as_NodeInfoResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 72U);
    CHECK_EQ(response->node_name()->str(), "/demo_node");
    REQUIRE(response->publishers() != nullptr);
    REQUIRE(response->subscribers() != nullptr);
    REQUIRE(response->services() != nullptr);
    CHECK_EQ(response->publishers()->size(), 1U);
    CHECK_EQ(response->subscribers()->size(), 1U);
    CHECK_EQ(response->services()->size(), 1U);
    CHECK_EQ(response->publishers()->Get(0)->name()->str(), "/topic_pub");
    CHECK_EQ(response->publishers()->Get(0)->type()->str(),
             "std_msgs/msg/String");
    CHECK_EQ(response->subscribers()->Get(0)->name()->str(), "/topic_sub");
    CHECK_EQ(response->subscribers()->Get(0)->type()->str(),
             "std_msgs/msg/Int32");
    CHECK_EQ(response->services()->Get(0)->name()->str(), "/service");
    CHECK_EQ(response->services()->Get(0)->type()->str(),
             "std_srvs/srv/Trigger");
    CHECK(response->found());
    CHECK_EQ(out.CommandCount<NodeInfoResponseCmd>(), 0U);
  }
}
