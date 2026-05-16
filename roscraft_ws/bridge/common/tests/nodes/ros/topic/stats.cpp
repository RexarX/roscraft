#include <doctest/doctest.h>

#include <sstream>

#include <roscraft/bridge/nodes/ros/details/introspection_codec.hpp>

#define private public
#include <roscraft/bridge/nodes/ros/topic/stats.hpp>
#undef private

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/error.hpp>
#include <roscraft/bridge/command/types/topic.hpp>

#include <rclcpp/rclcpp.hpp>

#include <cstring>
#include <memory>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace roscraft::bridge;

namespace {

using HzBwData = TopicStatsNode::HzBwData;
using DelayData = TopicStatsNode::DelayData;

class ScopedRosContext {
public:
  ScopedRosContext() {
    if (!rclcpp::ok()) {
      int argc = 0;
      rclcpp::init(argc, nullptr);
      owns_context_ = true;
    }
  }

  ScopedRosContext(const ScopedRosContext&) = delete;
  ScopedRosContext(ScopedRosContext&&) = delete;
  ~ScopedRosContext() {
    if (owns_context_ && rclcpp::ok()) {
      rclcpp::shutdown();
    }
  }

  ScopedRosContext& operator=(const ScopedRosContext&) = delete;
  ScopedRosContext& operator=(ScopedRosContext&&) = delete;

private:
  bool owns_context_ = false;
};

void RegisterQueues(CommandQueue& incoming, CommandQueue& outgoing) {
  incoming.Register<TopicHzCmd>();
  incoming.Register<TopicBwCmd>();
  incoming.Register<TopicDelayCmd>();
  incoming.Register<TopicStatsStopAllCmd>();
  outgoing.Register<TopicHzResponseCmd>();
  outgoing.Register<TopicBwResponseCmd>();
  outgoing.Register<TopicDelayResponseCmd>();
  outgoing.Register<ErrorCmd>();
}

[[nodiscard]] auto ToSerializedMessage(std::span<const uint8_t> payload)
    -> rclcpp::SerializedMessage {
  rclcpp::SerializedMessage serialized_message(payload.size());
  auto& rcl_serialized = serialized_message.get_rcl_serialized_message();
  if (!payload.empty()) {
    std::memcpy(rcl_serialized.buffer, payload.data(), payload.size());
  }
  rcl_serialized.buffer_length = payload.size();
  return serialized_message;
}

}  // namespace

TEST_SUITE("bridge::TopicStatsNode") {
  TEST_CASE("bridge::TopicStatsNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    CHECK(std::string_view(node.get_name()) == "roscraft_topic_stats_node");
    CHECK_NE(node.drain_timer_, nullptr);
    CHECK_NE(node.report_timer_, nullptr);
    CHECK_EQ(node.sessions_.size(), 0);
  }

  TEST_CASE("bridge::TopicStatsNode::StartHzSession invalid input") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicHzCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 1;
    cmd.topic_name = "";
    cmd.message_type = "std_msgs/msg/String";
    node.StartHzSession(cmd);

    CHECK_EQ(node.sessions_.size(), 0);

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 1);
    CHECK_EQ(err.error_code, "TOPIC_HZ_FAILED");
  }

  TEST_CASE("bridge::TopicStatsNode::StartBwSession invalid input") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicBwCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 2;
    cmd.topic_name = "/some_topic";
    cmd.message_type = "";
    node.StartBwSession(cmd);

    CHECK_EQ(node.sessions_.size(), 0);

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 2);
    CHECK_EQ(err.error_code, "TOPIC_BW_FAILED");
  }

  TEST_CASE("bridge::TopicStatsNode::StartHzSession with invalid type") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicHzCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 3;
    cmd.topic_name = "/bad_type_topic";
    cmd.message_type = "not_a_valid_type";
    node.StartHzSession(cmd);

    CHECK_EQ(node.sessions_.size(), 0);

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 3);
    CHECK_EQ(err.error_code, "STATS_FAILED");
  }

  TEST_CASE("bridge::TopicStatsNode::DrainHzCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicHzCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 10;
    cmd.topic_name = "/stats_hz_test";
    cmd.message_type = "std_msgs/msg/String";
    cmd.window = 15;

    incoming.Enqueue(std::move(cmd));

    node.DrainHzCommands();

    CHECK(node.sessions_.contains("/stats_hz_test#hz"));
    const auto& session = node.sessions_.at("/stats_hz_test#hz");
    CHECK_EQ(session.request_id, 10);
    CHECK_EQ(session.window, 15);
    const auto* hz_bw_data = std::get_if<HzBwData>(&session.data);
    REQUIRE_NE(hz_bw_data, nullptr);
    CHECK(hz_bw_data->is_hz);
    CHECK_FALSE(hz_bw_data->wall_time);
    CHECK_EQ(hz_bw_data->timestamps_ns.size(), 0);
  }

  TEST_CASE("bridge::TopicStatsNode::DrainBwCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicBwCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 20;
    cmd.topic_name = "/stats_bw_test";
    cmd.message_type = "std_msgs/msg/String";
    cmd.window = 25;

    incoming.Enqueue(std::move(cmd));

    node.DrainBwCommands();

    CHECK(node.sessions_.contains("/stats_bw_test#bw"));
    const auto& session = node.sessions_.at("/stats_bw_test#bw");
    CHECK_EQ(session.request_id, 20);
    CHECK_EQ(session.window, 25);
    const auto* bw_data = std::get_if<HzBwData>(&session.data);
    REQUIRE_NE(bw_data, nullptr);
    CHECK_FALSE(bw_data->is_hz);
    CHECK_FALSE(bw_data->wall_time);
    CHECK_EQ(bw_data->timestamps_ns.size(), 0);
  }

  TEST_CASE("bridge::TopicStatsNode::StartDelaySession invalid input") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicDelayCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 70;
    cmd.topic_name = "";
    cmd.message_type = "geometry_msgs/msg/PointStamped";
    node.StartDelaySession(cmd);

    CHECK_EQ(node.sessions_.size(), 0);

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 70);
    CHECK_EQ(err.error_code, "TOPIC_DELAY_FAILED");
  }

  TEST_CASE("bridge::TopicStatsNode::StartDelaySession unsupported type") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicDelayCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 71;
    cmd.topic_name = "/delay_unsupported";
    cmd.message_type = "std_msgs/msg/String";
    node.StartDelaySession(cmd);

    CHECK_EQ(node.sessions_.size(), 0);

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 71);
    CHECK_EQ(err.error_code, "TOPIC_DELAY_FAILED");
  }

  TEST_CASE("bridge::TopicStatsNode::DrainDelayCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicDelayCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 72;
    cmd.topic_name = "/stats_delay_test";
    cmd.message_type = "geometry_msgs/msg/PointStamped";
    cmd.window = 12;

    incoming.Enqueue(std::move(cmd));

    node.DrainDelayCommands();

    CHECK(node.sessions_.contains("/stats_delay_test#delay"));
    const auto& session = node.sessions_.at("/stats_delay_test#delay");
    CHECK_EQ(session.request_id, 72);
    CHECK_EQ(session.window, 12);
    const auto* delay_data = std::get_if<DelayData>(&session.data);
    REQUIRE_NE(delay_data, nullptr);
    CHECK_EQ(delay_data->delays_seconds.size(), 0);
  }

  TEST_CASE("bridge::TopicStatsNode::StartDelaySession resets existing") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicDelayCmd first(std::pmr::get_default_resource());
    first.request_id = 73;
    first.topic_name = "/reset_delay_test";
    first.message_type = "geometry_msgs/msg/PointStamped";
    first.window = 10;
    node.StartDelaySession(first);

    CHECK(node.sessions_.contains("/reset_delay_test#delay"));

    auto& seeded = node.sessions_.at("/reset_delay_test#delay");
    auto* seeded_delay = std::get_if<DelayData>(&seeded.data);
    REQUIRE_NE(seeded_delay, nullptr);
    seeded_delay->delays_seconds.push_back(0.1);
    seeded_delay->delays_seconds.push_back(0.2);

    TopicDelayCmd second(std::pmr::get_default_resource());
    second.request_id = 74;
    second.topic_name = "/reset_delay_test";
    second.message_type = "";
    second.window = 20;
    node.StartDelaySession(second);

    const auto& session = node.sessions_.at("/reset_delay_test#delay");
    CHECK_EQ(session.request_id, 74);
    CHECK_EQ(session.window, 20);
    const auto* delay_data = std::get_if<DelayData>(&session.data);
    REQUIRE_NE(delay_data, nullptr);
    CHECK_EQ(delay_data->delays_seconds.size(), 0);
    CHECK_EQ(node.sessions_.size(), 1);
    CHECK_EQ(session.message_type, "geometry_msgs/msg/PointStamped");

    ErrorCmd err;
    CHECK_FALSE(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
  }

  TEST_CASE("bridge::TopicStatsNode::ReportStats emits delay response") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicDelayCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 75;
    cmd.topic_name = "/delay_report_test";
    cmd.message_type = "geometry_msgs/msg/PointStamped";
    cmd.window = 5;
    node.StartDelaySession(cmd);

    auto& session = node.sessions_.at("/delay_report_test#delay");
    auto* delay_data = std::get_if<DelayData>(&session.data);
    REQUIRE_NE(delay_data, nullptr);
    delay_data->delays_seconds.push_back(0.1);
    delay_data->delays_seconds.push_back(0.2);
    delay_data->delays_seconds.push_back(0.3);

    node.ReportStats();

    TopicDelayResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<TopicDelayResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 75);
    CHECK_EQ(response.topic_name, "/delay_report_test");
    CHECK_EQ(response.average_delay, doctest::Approx(0.2));
    CHECK_EQ(response.min_delay, doctest::Approx(0.1));
    CHECK_EQ(response.max_delay, doctest::Approx(0.3));
    CHECK_EQ(response.window, 5U);
    CHECK_EQ(response.message_count, 3U);
  }

  TEST_CASE("bridge::TopicStatsNode::ExtractHeaderStampSeconds") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    SUBCASE("Extracts seconds from header stamp layout") {
      const auto introspection =
          details::LoadMessageIntrospection("geometry_msgs/msg/PointStamped");
      REQUIRE(introspection.has_value());

      constexpr std::string_view kYaml =
          "header:\n"
          "  stamp:\n"
          "    sec: 42\n"
          "    nanosec: 250000000\n"
          "  frame_id: map\n"
          "point:\n"
          "  x: 1.0\n"
          "  y: 2.0\n"
          "  z: 3.0\n";
      const auto payload = details::SerializeYamlToCdr(kYaml, *introspection);
      REQUIRE(payload.has_value());

      const auto serialized_message = ToSerializedMessage(*payload);
      const auto stamp_seconds = node.ExtractHeaderStampSeconds(
          "geometry_msgs/msg/PointStamped", serialized_message);
      REQUIRE(stamp_seconds.has_value());
      CHECK_EQ(*stamp_seconds, doctest::Approx(42.25));
    }

    SUBCASE("Extracts seconds from builtin time layout") {
      const auto introspection =
          details::LoadMessageIntrospection("builtin_interfaces/msg/Time");
      REQUIRE(introspection.has_value());

      constexpr std::string_view kYaml =
          "sec: 12\n"
          "nanosec: 500000000\n";
      const auto payload = details::SerializeYamlToCdr(kYaml, *introspection);
      REQUIRE(payload.has_value());

      const auto serialized_message = ToSerializedMessage(*payload);
      const auto stamp_seconds = node.ExtractHeaderStampSeconds(
          "builtin_interfaces/msg/Time", serialized_message);
      REQUIRE(stamp_seconds.has_value());
      CHECK_EQ(*stamp_seconds, doctest::Approx(12.5));
    }

    SUBCASE("Returns nullopt for invalid serialized payload") {
      const std::vector<uint8_t> payload{0x1, 0x2, 0x3, 0x4};
      const auto serialized_message = ToSerializedMessage(payload);

      const auto stamp_seconds = node.ExtractHeaderStampSeconds(
          "builtin_interfaces/msg/Time", serialized_message);
      CHECK_FALSE(stamp_seconds.has_value());
    }
  }

  TEST_CASE(
      "bridge::TopicStatsNode::EnsureDelayStampExtractor caches supported "
      "type") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    const auto* first =
        node.EnsureDelayStampExtractor("geometry_msgs/msg/PointStamped");
    REQUIRE_NE(first, nullptr);
    REQUIRE(node.delay_stamp_extractors_.contains(
        "geometry_msgs/msg/PointStamped"));
    CHECK(node.delay_stamp_extractors_.at("geometry_msgs/msg/PointStamped")
              .has_value());

    const auto* second =
        node.EnsureDelayStampExtractor("geometry_msgs/msg/PointStamped");
    REQUIRE_NE(second, nullptr);
    CHECK_EQ(second, first);
  }

  TEST_CASE(
      "bridge::TopicStatsNode::EnsureDelayStampExtractor caches unsupported "
      "type") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    const auto* first = node.EnsureDelayStampExtractor("std_msgs/msg/String");
    CHECK_EQ(first, nullptr);
    REQUIRE(node.delay_stamp_extractors_.contains("std_msgs/msg/String"));
    CHECK_FALSE(
        node.delay_stamp_extractors_.at("std_msgs/msg/String").has_value());

    const size_t cached_size = node.delay_stamp_extractors_.size();
    const auto* second = node.EnsureDelayStampExtractor("std_msgs/msg/String");
    CHECK_EQ(second, nullptr);
    CHECK_EQ(node.delay_stamp_extractors_.size(), cached_size);
  }

  TEST_CASE(
      "bridge::TopicStatsNode::StartDelaySession supports builtin time type") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicDelayCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 76;
    cmd.topic_name = "/time_delay_test";
    cmd.message_type = "builtin_interfaces/msg/Time";
    cmd.window = 8;

    node.StartDelaySession(cmd);

    CHECK(node.sessions_.contains("/time_delay_test#delay"));
    const auto& session = node.sessions_.at("/time_delay_test#delay");
    CHECK_EQ(session.request_id, 76);
    CHECK_EQ(session.window, 8U);
    CHECK(std::holds_alternative<DelayData>(session.data));
    CHECK_EQ(session.message_type, "builtin_interfaces/msg/Time");

    ErrorCmd err;
    CHECK_FALSE(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
  }

  TEST_CASE("bridge::TopicStatsNode::StartHzSession resets existing") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicHzCmd first(std::pmr::get_default_resource());
    first.request_id = 30;
    first.topic_name = "/reset_hz_test";
    first.message_type = "std_msgs/msg/String";
    first.window = 10;
    node.StartHzSession(first);

    CHECK(node.sessions_.contains("/reset_hz_test#hz"));

    TopicHzCmd second(std::pmr::get_default_resource());
    second.request_id = 31;
    second.topic_name = "/reset_hz_test";
    second.message_type = "";
    second.window = 20;
    second.wall_time = true;
    node.StartHzSession(second);

    const auto& session = node.sessions_.at("/reset_hz_test#hz");
    CHECK_EQ(session.request_id, 31);
    CHECK_EQ(session.window, 20);
    const auto* hz_data = std::get_if<HzBwData>(&session.data);
    REQUIRE_NE(hz_data, nullptr);
    CHECK(hz_data->wall_time);
    CHECK_EQ(node.sessions_.size(), 1);
    CHECK_EQ(session.message_type, "std_msgs/msg/String");

    ErrorCmd err;
    CHECK_FALSE(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
  }

  TEST_CASE("bridge::TopicStatsNode::StartBwSession resets existing") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicBwCmd first(std::pmr::get_default_resource());
    first.request_id = 32;
    first.topic_name = "/reset_bw_test";
    first.message_type = "std_msgs/msg/String";
    first.window = 10;
    node.StartBwSession(first);

    CHECK(node.sessions_.contains("/reset_bw_test#bw"));

    TopicBwCmd second(std::pmr::get_default_resource());
    second.request_id = 33;
    second.topic_name = "/reset_bw_test";
    second.message_type = "";
    second.window = 20;
    second.wall_time = true;
    node.StartBwSession(second);

    const auto& session = node.sessions_.at("/reset_bw_test#bw");
    CHECK_EQ(session.request_id, 33);
    CHECK_EQ(session.window, 20);
    const auto* bw_data = std::get_if<HzBwData>(&session.data);
    REQUIRE_NE(bw_data, nullptr);
    CHECK(bw_data->wall_time);
    CHECK_FALSE(bw_data->is_hz);
    CHECK_EQ(node.sessions_.size(), 1);
    CHECK_EQ(session.message_type, "std_msgs/msg/String");

    ErrorCmd err;
    CHECK_FALSE(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
  }

  TEST_CASE(
      "bridge::TopicStatsNode::StartHzSession type mismatch sends error") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicHzCmd first(std::pmr::get_default_resource());
    first.request_id = 40;
    first.topic_name = "/mismatch_topic";
    first.message_type = "std_msgs/msg/String";
    node.StartHzSession(first);

    TopicHzCmd second(std::pmr::get_default_resource());
    second.request_id = 41;
    second.topic_name = "/mismatch_topic";
    second.message_type = "std_msgs/msg/Int32";
    node.StartHzSession(second);

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 41);
    CHECK_EQ(err.error_code, "TOPIC_HZ_FAILED");
  }

  TEST_CASE("bridge::TopicStatsNode::StartHzSession window=0 stops session") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicHzCmd start(std::pmr::get_default_resource());
    start.request_id = 50;
    start.topic_name = "/stop_test";
    start.message_type = "std_msgs/msg/String";
    start.window = 10;
    node.StartHzSession(start);

    CHECK(node.sessions_.contains("/stop_test#hz"));

    TopicHzCmd stop(std::pmr::get_default_resource());
    stop.request_id = 51;
    stop.topic_name = "/stop_test";
    stop.message_type = "";
    stop.window = 0;
    node.StartHzSession(stop);

    CHECK_FALSE(node.sessions_.contains("/stop_test#hz"));
    CHECK_EQ(node.sessions_.size(), 0);

    ErrorCmd err;
    CHECK_FALSE(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
  }

  TEST_CASE("bridge::TopicStatsNode::StartBwSession window=0 stops session") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicBwCmd start(std::pmr::get_default_resource());
    start.request_id = 52;
    start.topic_name = "/stop_bw_test";
    start.message_type = "std_msgs/msg/String";
    start.window = 10;
    node.StartBwSession(start);

    CHECK(node.sessions_.contains("/stop_bw_test#bw"));

    TopicBwCmd stop(std::pmr::get_default_resource());
    stop.request_id = 53;
    stop.topic_name = "/stop_bw_test";
    stop.message_type = "";
    stop.window = 0;
    node.StartBwSession(stop);

    CHECK_FALSE(node.sessions_.contains("/stop_bw_test#bw"));
    CHECK_EQ(node.sessions_.size(), 0);
  }

  TEST_CASE(
      "bridge::TopicStatsNode::StartDelaySession window=0 stops session") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicDelayCmd start(std::pmr::get_default_resource());
    start.request_id = 54;
    start.topic_name = "/stop_delay_test";
    start.message_type = "geometry_msgs/msg/PointStamped";
    start.window = 10;
    node.StartDelaySession(start);

    CHECK(node.sessions_.contains("/stop_delay_test#delay"));

    TopicDelayCmd stop(std::pmr::get_default_resource());
    stop.request_id = 55;
    stop.topic_name = "/stop_delay_test";
    stop.message_type = "";
    stop.window = 0;
    node.StartDelaySession(stop);

    CHECK_FALSE(node.sessions_.contains("/stop_delay_test#delay"));
    CHECK_EQ(node.sessions_.size(), 0);
  }

  TEST_CASE("bridge::TopicStatsNode::StopAllSessions clears all sessions") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicHzCmd hz_cmd(std::pmr::get_default_resource());
    hz_cmd.request_id = 56;
    hz_cmd.topic_name = "/topic_a";
    hz_cmd.message_type = "std_msgs/msg/String";
    hz_cmd.window = 10;
    node.StartHzSession(hz_cmd);

    TopicDelayCmd delay_cmd(std::pmr::get_default_resource());
    delay_cmd.request_id = 57;
    delay_cmd.topic_name = "/topic_b";
    delay_cmd.message_type = "geometry_msgs/msg/PointStamped";
    delay_cmd.window = 10;
    node.StartDelaySession(delay_cmd);

    CHECK_EQ(node.sessions_.size(), 2);

    node.StopAllSessions();

    CHECK_EQ(node.sessions_.size(), 0);
  }

  TEST_CASE("bridge::TopicStatsNode::DrainStopAllCommands stops all sessions") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicHzCmd hz_cmd(std::pmr::get_default_resource());
    hz_cmd.request_id = 58;
    hz_cmd.topic_name = "/topic_c";
    hz_cmd.message_type = "std_msgs/msg/String";
    hz_cmd.window = 10;
    node.StartHzSession(hz_cmd);

    CHECK_EQ(node.sessions_.size(), 1);

    incoming.Enqueue(TopicStatsStopAllCmd{});

    node.DrainStopAllCommands();

    CHECK_EQ(node.sessions_.size(), 0);
  }

  TEST_CASE("bridge::TopicStatsNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing, std::pmr::get_default_resource());

    node.OnReportTimer();
    CHECK_EQ(node.sessions_.size(), 0);
  }
}
