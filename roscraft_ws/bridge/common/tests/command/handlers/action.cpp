#include <doctest/doctest.h>

#include <roscraft/bridge/command/handler/action.hpp>

#include "test_utils.hpp"

#include <array>
#include <memory_resource>

using namespace roscraft::bridge;

namespace {

[[nodiscard]] auto BuildActionInfoPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::ActionInfoPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateActionInfoPacketDirect(
                                  fbb, 101U, "/demo/action", true);
                            });
}

[[nodiscard]] auto BuildActionSendGoalPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(
      fbs::PacketPayload::ActionSendGoalPacket,
      [](flatbuffers::FlatBufferBuilder& fbb) {
        const auto action_name = fbb.CreateString("/demo/action");
        const auto action_type =
            fbb.CreateString("example_interfaces/action/Fibonacci");
        const std::array<uint8_t, 3> goal_payload{9U, 8U, 7U};
        const auto goal_payload_offset =
            fbb.CreateVector(goal_payload.data(), goal_payload.size());
        return fbs::CreateActionSendGoalPacket(fbb, 202U, action_name,
                                               action_type, goal_payload_offset,
                                               true, 4.5);
      });
}

}  // namespace

TEST_SUITE("bridge::action handlers") {
  TEST_CASE("bridge::ActionInfoHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ActionInfoCmd>();
    out.Register<ActionInfoResponseCmd>();

    auto handler = ActionInfoHandler::From(in, out);

    const auto bytes = BuildActionInfoPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    ActionInfoCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<ActionInfoCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 101U);
    CHECK_EQ(cmd.action_name, "/demo/action");
    CHECK(cmd.include_hidden);
  }

  TEST_CASE("bridge::ActionInfoHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ActionInfoCmd>();
    out.Register<ActionInfoResponseCmd>();

    auto handler = ActionInfoHandler::From(in, out);

    ActionInfoResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 303U;
    cmd.action_name = "/demo/action";
    cmd.action_type = "example_interfaces/action/Fibonacci";
    cmd.client_count = 1U;
    cmd.server_count = 2U;
    cmd.feedback_publisher_count = 3U;
    cmd.feedback_subscriber_count = 4U;
    cmd.status_publisher_count = 5U;
    cmd.status_subscriber_count = 6U;
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::ActionInfoResponsePacket);

    const auto* response = packet->payload_as_ActionInfoResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 303U);
    CHECK_EQ(response->action_name()->str(), "/demo/action");
    CHECK_EQ(response->action_type()->str(),
             "example_interfaces/action/Fibonacci");
    CHECK_EQ(response->client_count(), 1U);
    CHECK_EQ(response->server_count(), 2U);
    CHECK_EQ(response->feedback_publisher_count(), 3U);
    CHECK_EQ(response->feedback_subscriber_count(), 4U);
    CHECK_EQ(response->status_publisher_count(), 5U);
    CHECK_EQ(response->status_subscriber_count(), 6U);
    CHECK_EQ(out.CommandCount<ActionInfoResponseCmd>(), 0U);
  }

  TEST_CASE("bridge::ActionSendGoalHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ActionSendGoalCmd>();
    out.Register<ActionResultCmd>();

    auto handler = ActionSendGoalHandler::From(in, out);

    const auto bytes = BuildActionSendGoalPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    ActionSendGoalCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<ActionSendGoalCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 202U);
    CHECK_EQ(cmd.action_name, "/demo/action");
    CHECK_EQ(cmd.action_type, "example_interfaces/action/Fibonacci");
    CHECK_EQ(cmd.goal_payload.size(), 3U);
    CHECK_EQ(cmd.goal_payload[0], 9U);
    CHECK_EQ(cmd.goal_payload[1], 8U);
    CHECK_EQ(cmd.goal_payload[2], 7U);
    CHECK(cmd.feedback);
    CHECK_EQ(cmd.timeout_seconds, doctest::Approx(4.5));
  }

  TEST_CASE("bridge::ActionSendGoalHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ActionSendGoalCmd>();
    out.Register<ActionResultCmd>();

    auto handler = ActionSendGoalHandler::From(in, out);

    ActionResultCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 404U;
    cmd.action_name = "/demo/action";
    cmd.action_type = "example_interfaces/action/Fibonacci";
    cmd.result_payload = {1U, 2U, 3U, 4U};
    cmd.result_text = "goal finished";
    cmd.success = true;
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::ActionResultPacket);

    const auto* response = packet->payload_as_ActionResultPacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 404U);
    CHECK_EQ(response->action_name()->str(), "/demo/action");
    CHECK_EQ(response->action_type()->str(),
             "example_interfaces/action/Fibonacci");
    CHECK(response->success());
    REQUIRE(response->result_payload() != nullptr);
    CHECK_EQ(response->result_payload()->size(), 4U);
    CHECK_EQ(response->result_payload()->Get(0), 1U);
    CHECK_EQ(response->result_payload()->Get(3), 4U);
    CHECK_EQ(response->result_text()->str(), "goal finished");
    CHECK_EQ(out.CommandCount<ActionResultCmd>(), 0U);
  }

  TEST_CASE("bridge::ActionFeedbackHandler::DrainAndFlush") {
    CommandQueue out;
    out.Register<ActionFeedbackCmd>();

    auto handler = ActionFeedbackHandler::From(out);

    ActionFeedbackCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 505U;
    cmd.action_name = "/demo/action";
    cmd.action_type = "example_interfaces/action/Fibonacci";
    cmd.feedback_payload = {5U, 6U};
    cmd.feedback_text = "halfway";
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::ActionFeedbackPacket);

    const auto* response = packet->payload_as_ActionFeedbackPacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 505U);
    CHECK_EQ(response->action_name()->str(), "/demo/action");
    CHECK_EQ(response->action_type()->str(),
             "example_interfaces/action/Fibonacci");
    REQUIRE(response->feedback_payload() != nullptr);
    CHECK_EQ(response->feedback_payload()->size(), 2U);
    CHECK_EQ(response->feedback_payload()->Get(0), 5U);
    CHECK_EQ(response->feedback_payload()->Get(1), 6U);
    CHECK_EQ(response->feedback_text()->str(), "halfway");
    CHECK_EQ(out.CommandCount<ActionFeedbackCmd>(), 0U);
  }
}
