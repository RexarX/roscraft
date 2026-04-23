#include <doctest/doctest.h>

#include <roscraft/bridge/command/handler/topic.hpp>

#include "test_utils.hpp"

#include <array>
#include <memory_resource>

using namespace roscraft::bridge;

namespace {

[[nodiscard]] auto BuildTopicInfoPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::TopicInfoPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateTopicInfoPacketDirect(fbb, 10U,
                                                                      "/topic");
                            });
}

[[nodiscard]] auto BuildTopicSubscribePacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::TopicSubscribePacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateTopicSubscribePacketDirect(
                                  fbb, 20U, "/topic", "std_msgs/msg/String",
                                  true, 1.5, true);
                            });
}

[[nodiscard]] auto BuildTopicPublishMessagePacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(
      fbs::PacketPayload::TopicPublishMessagePacket,
      [](flatbuffers::FlatBufferBuilder& fbb) {
        const auto topic_name = fbb.CreateString("/topic");
        const auto message_type = fbb.CreateString("std_msgs/msg/String");
        const std::array<uint8_t, 3> payload{1U, 2U, 3U};
        const auto payload_offset =
            fbb.CreateVector(payload.data(), payload.size());
        const auto qos = fbb.CreateString("sensor_data");
        return fbs::CreateTopicPublishMessagePacket(
            fbb, 30U, topic_name, message_type, payload_offset, false, 10.0, 2U,
            qos);
      });
}

[[nodiscard]] auto BuildTopicHzPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::TopicHzPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateTopicHzPacketDirect(
                                  fbb, 40U, "/topic", "std_msgs/msg/String",
                                  15U, true);
                            });
}

[[nodiscard]] auto BuildTopicBwPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::TopicBwPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateTopicBwPacketDirect(
                                  fbb, 50U, "/topic", "std_msgs/msg/String",
                                  20U, false);
                            });
}

[[nodiscard]] auto BuildTopicDelayPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::TopicDelayPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateTopicDelayPacketDirect(
                                  fbb, 60U, "/topic",
                                  "geometry_msgs/msg/PointStamped", 12U);
                            });
}

}  // namespace

TEST_SUITE("bridge::topic handlers") {
  TEST_CASE("bridge::TopicInfoHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<TopicInfoCmd>();
    out.Register<TopicInfoResponseCmd>();

    auto handler = TopicInfoHandler::From(in, out);

    const auto bytes = BuildTopicInfoPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    TopicInfoCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<TopicInfoCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 10U);
    CHECK_EQ(cmd.topic_name, "/topic");
  }

  TEST_CASE("bridge::TopicInfoHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<TopicInfoCmd>();
    out.Register<TopicInfoResponseCmd>();

    auto handler = TopicInfoHandler::From(in, out);

    TopicInfoResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 11U;
    cmd.topic_name = "/topic";
    cmd.message_type = "std_msgs/msg/String";
    cmd.publisher_count = 2U;
    cmd.subscriber_count = 1U;
    cmd.publisher_nodes.emplace_back("/pub1");
    cmd.publisher_nodes.emplace_back("/pub2");
    cmd.subscriber_nodes.emplace_back("/sub1");
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::TopicInfoResponsePacket);

    const auto* response = packet->payload_as_TopicInfoResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 11U);
    CHECK_EQ(response->topic_name()->str(), "/topic");
    CHECK_EQ(response->message_type()->str(), "std_msgs/msg/String");
    CHECK_EQ(response->publisher_count(), 2U);
    CHECK_EQ(response->subscriber_count(), 1U);
    REQUIRE(response->publisher_nodes() != nullptr);
    REQUIRE(response->subscriber_nodes() != nullptr);
    CHECK_EQ(response->publisher_nodes()->size(), 2U);
    CHECK_EQ(response->subscriber_nodes()->size(), 1U);
    CHECK_EQ(response->publisher_nodes()->Get(0)->str(), "/pub1");
    CHECK_EQ(response->publisher_nodes()->Get(1)->str(), "/pub2");
    CHECK_EQ(response->subscriber_nodes()->Get(0)->str(), "/sub1");
    CHECK_EQ(out.CommandCount<TopicInfoResponseCmd>(), 0U);
  }

  TEST_CASE("bridge::TopicSubscribeHandler::Receive") {
    CommandQueue in;
    in.Register<TopicSubscribeCmd>();

    auto handler = TopicSubscribeHandler::From(in);

    const auto bytes = BuildTopicSubscribePacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    TopicSubscribeCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<TopicSubscribeCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 20U);
    CHECK_EQ(cmd.topic_name, "/topic");
    CHECK_EQ(cmd.message_type, "std_msgs/msg/String");
    CHECK(cmd.once);
    CHECK(cmd.raw);
    CHECK_EQ(cmd.timeout_seconds, doctest::Approx(1.5));
  }

  TEST_CASE("bridge::TopicPublishMessageHandler::Receive") {
    CommandQueue in;
    in.Register<TopicPublishMessageCmd>();

    auto handler = TopicPublishMessageHandler::From(in);

    const auto bytes = BuildTopicPublishMessagePacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    TopicPublishMessageCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<TopicPublishMessageCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 30U);
    CHECK_EQ(cmd.topic_name, "/topic");
    CHECK_EQ(cmd.message_type, "std_msgs/msg/String");
    CHECK_EQ(cmd.payload.size(), 3U);
    CHECK_EQ(cmd.payload[0], 1U);
    CHECK_EQ(cmd.payload[2], 3U);
    CHECK_FALSE(cmd.once);
    CHECK_EQ(cmd.rate_hz, doctest::Approx(10.0));
    CHECK_EQ(cmd.times, 2U);
    CHECK_EQ(cmd.qos_profile, "sensor_data");
  }

  TEST_CASE("bridge::TopicHzHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<TopicHzCmd>();
    out.Register<TopicHzResponseCmd>();

    auto handler = TopicHzHandler::From(in, out);

    const auto bytes = BuildTopicHzPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    TopicHzCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<TopicHzCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 40U);
    CHECK_EQ(cmd.topic_name, "/topic");
    CHECK_EQ(cmd.message_type, "std_msgs/msg/String");
    CHECK_EQ(cmd.window, 15U);
    CHECK(cmd.wall_time);
  }

  TEST_CASE("bridge::TopicHzHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<TopicHzCmd>();
    out.Register<TopicHzResponseCmd>();

    auto handler = TopicHzHandler::From(in, out);

    TopicHzResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 41U;
    cmd.topic_name = "/topic";
    cmd.frequency = 30.5;
    cmd.window = 10U;
    cmd.message_count = 42U;
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::TopicHzResponsePacket);

    const auto* response = packet->payload_as_TopicHzResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 41U);
    CHECK_EQ(response->topic_name()->str(), "/topic");
    CHECK_EQ(response->frequency(), doctest::Approx(30.5));
    CHECK_EQ(response->window(), 10U);
    CHECK_EQ(response->message_count(), 42U);
    CHECK_EQ(out.CommandCount<TopicHzResponseCmd>(), 0U);
  }

  TEST_CASE("bridge::TopicBwHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<TopicBwCmd>();
    out.Register<TopicBwResponseCmd>();

    auto handler = TopicBwHandler::From(in, out);

    const auto bytes = BuildTopicBwPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    TopicBwCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<TopicBwCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 50U);
    CHECK_EQ(cmd.topic_name, "/topic");
    CHECK_EQ(cmd.message_type, "std_msgs/msg/String");
    CHECK_EQ(cmd.window, 20U);
    CHECK_FALSE(cmd.wall_time);
  }

  TEST_CASE("bridge::TopicBwHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<TopicBwCmd>();
    out.Register<TopicBwResponseCmd>();

    auto handler = TopicBwHandler::From(in, out);

    TopicBwResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 51U;
    cmd.topic_name = "/topic";
    cmd.bytes_per_second = 1024.0;
    cmd.window = 8U;
    cmd.message_count = 4U;
    cmd.total_bytes = 4096U;
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::TopicBwResponsePacket);

    const auto* response = packet->payload_as_TopicBwResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 51U);
    CHECK_EQ(response->topic_name()->str(), "/topic");
    CHECK_EQ(response->bytes_per_second(), doctest::Approx(1024.0));
    CHECK_EQ(response->window(), 8U);
    CHECK_EQ(response->message_count(), 4U);
    CHECK_EQ(response->total_bytes(), 4096U);
    CHECK_EQ(out.CommandCount<TopicBwResponseCmd>(), 0U);
  }

  TEST_CASE("bridge::TopicDelayHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<TopicDelayCmd>();
    out.Register<TopicDelayResponseCmd>();

    auto handler = TopicDelayHandler::From(in, out);

    const auto bytes = BuildTopicDelayPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    TopicDelayCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<TopicDelayCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 60U);
    CHECK_EQ(cmd.topic_name, "/topic");
    CHECK_EQ(cmd.message_type, "geometry_msgs/msg/PointStamped");
    CHECK_EQ(cmd.window, 12U);
  }

  TEST_CASE("bridge::TopicDelayHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<TopicDelayCmd>();
    out.Register<TopicDelayResponseCmd>();

    auto handler = TopicDelayHandler::From(in, out);

    TopicDelayResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 61U;
    cmd.topic_name = "/topic";
    cmd.average_delay = 0.12;
    cmd.min_delay = 0.05;
    cmd.max_delay = 0.20;
    cmd.window = 10U;
    cmd.message_count = 8U;
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::TopicDelayResponsePacket);

    const auto* response = packet->payload_as_TopicDelayResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 61U);
    CHECK_EQ(response->topic_name()->str(), "/topic");
    CHECK_EQ(response->average_delay(), doctest::Approx(0.12));
    CHECK_EQ(response->min_delay(), doctest::Approx(0.05));
    CHECK_EQ(response->max_delay(), doctest::Approx(0.20));
    CHECK_EQ(response->window(), 10U);
    CHECK_EQ(response->message_count(), 8U);
    CHECK_EQ(out.CommandCount<TopicDelayResponseCmd>(), 0U);
  }

  TEST_CASE("bridge::TopicPayloadHandler::DrainAndFlush") {
    CommandQueue out;
    out.Register<TopicPayloadCmd>();

    auto handler = TopicPayloadHandler::From(out);

    TopicPayloadCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 61U;
    cmd.topic_name = "/topic";
    cmd.message_type = "std_msgs/msg/String";
    cmd.payload = {7U, 8U, 9U};
    cmd.raw = true;
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::TopicPayloadPacket);

    const auto* response = packet->payload_as_TopicPayloadPacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 61U);
    CHECK_EQ(response->topic_name()->str(), "/topic");
    CHECK_EQ(response->message_type()->str(), "std_msgs/msg/String");
    CHECK(response->raw());
    REQUIRE(response->payload() != nullptr);
    CHECK_EQ(response->payload()->size(), 3U);
    CHECK_EQ(response->payload()->Get(0), 7U);
    CHECK_EQ(response->payload()->Get(2), 9U);
    CHECK_EQ(out.CommandCount<TopicPayloadCmd>(), 0U);
  }
}
