#include <doctest/doctest.h>

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/rclcpp.hpp>

#include <memory>
#include <memory_resource>
#include <string>

#define private public
#include <roscraft/bridge/nodes/topic_stats.hpp>
#undef private

using namespace roscraft::bridge;

namespace {

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
  outgoing.Register<TopicHzResponseCmd>();
  outgoing.Register<TopicBwResponseCmd>();
  outgoing.Register<ErrorCmd>();
}

}  // namespace

TEST_SUITE("bridge::TopicStatsNode") {
  TEST_CASE("bridge::TopicStatsNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    TopicStatsNode node(incoming, outgoing);

    CHECK_EQ(std::string_view(node.get_name()), "roscraft_topic_stats_node");
    CHECK_NE(node.drain_timer_, nullptr);
    CHECK_NE(node.report_timer_, nullptr);
    CHECK_EQ(node.sessions_.size(), 0);
  }

  TEST_CASE("bridge::TopicStatsNode::StartHzSession invalid input") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing);

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
    TopicStatsNode node(incoming, outgoing);

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
    TopicStatsNode node(incoming, outgoing);

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
    TopicStatsNode node(incoming, outgoing);

    TopicHzCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 10;
    cmd.topic_name = "/stats_hz_test";
    cmd.message_type = "std_msgs/msg/String";
    cmd.window = 15;

    incoming.Enqueue(std::move(cmd));

    node.DrainHzCommands();

    CHECK(node.sessions_.contains("/stats_hz_test"));
    const auto& session = node.sessions_.at("/stats_hz_test");
    CHECK_EQ(session.request_id, 10);
    CHECK_EQ(session.window, 15);
    CHECK(session.is_hz);
    CHECK_EQ(session.timestamps.size(), 0);
  }

  TEST_CASE("bridge::TopicStatsNode::DrainBwCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing);

    TopicBwCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 20;
    cmd.topic_name = "/stats_bw_test";
    cmd.message_type = "std_msgs/msg/String";
    cmd.window = 25;

    incoming.Enqueue(std::move(cmd));

    node.DrainBwCommands();

    CHECK(node.sessions_.contains("/stats_bw_test"));
    const auto& session = node.sessions_.at("/stats_bw_test");
    CHECK_EQ(session.request_id, 20);
    CHECK_EQ(session.window, 25);
    CHECK_FALSE(session.is_hz);
    CHECK_EQ(session.timestamps.size(), 0);
  }

  TEST_CASE("bridge::TopicStatsNode::StartHzSession resets existing") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing);

    TopicHzCmd first(std::pmr::get_default_resource());
    first.request_id = 30;
    first.topic_name = "/reset_hz_test";
    first.message_type = "std_msgs/msg/String";
    first.window = 10;
    node.StartHzSession(first);

    CHECK(node.sessions_.contains("/reset_hz_test"));

    TopicHzCmd second(std::pmr::get_default_resource());
    second.request_id = 31;
    second.topic_name = "/reset_hz_test";
    second.message_type = "std_msgs/msg/String";
    second.window = 20;
    node.StartHzSession(second);

    const auto& session = node.sessions_.at("/reset_hz_test");
    CHECK_EQ(session.request_id, 31);
    CHECK_EQ(session.window, 20);
    CHECK_EQ(node.sessions_.size(), 1);
  }

  TEST_CASE(
      "bridge::TopicStatsNode::StartHzSession type mismatch sends error") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing);

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

  TEST_CASE("bridge::TopicStatsNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicStatsNode node(incoming, outgoing);

    node.OnReportTimer();
    CHECK_EQ(node.sessions_.size(), 0);
  }
}
