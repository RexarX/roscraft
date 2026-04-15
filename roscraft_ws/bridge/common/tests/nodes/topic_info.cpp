#include <doctest/doctest.h>

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/rclcpp.hpp>

#include <algorithm>
#include <memory_resource>
#include <ranges>

#define private public
#include <roscraft/bridge/nodes/topic_info.hpp>
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
  incoming.Register<TopicInfoCmd>();
  outgoing.Register<TopicInfoResponseCmd>();
}

}  // namespace

TEST_SUITE("bridge::TopicInfoNode") {
  TEST_CASE("bridge::TopicInfoNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    TopicInfoNode node(incoming, outgoing);

    CHECK_EQ(std::string_view(node.get_name()), "roscraft_topic_info_node");
    CHECK_NE(node.poll_timer_, nullptr);
  }

  TEST_CASE("bridge::TopicInfoNode::DrainTopicInfoCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicInfoNode node(incoming, outgoing);

    TopicInfoCmd query(std::pmr::get_default_resource());
    query.request_id = 41;
    query.topic_name = "/rosout";
    incoming.Enqueue(std::move(query));

    node.DrainTopicInfoCommands();

    TopicInfoResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<TopicInfoResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 41);
    CHECK_EQ(response.topic_name, "/rosout");
    CHECK_GE(response.publisher_count, 0);
    CHECK_GE(response.subscriber_count, 0);
    CHECK_LE(response.publisher_nodes.size(), response.publisher_count);
    CHECK_LE(response.subscriber_nodes.size(), response.subscriber_count);
    CHECK(std::ranges::is_sorted(response.publisher_nodes));
    CHECK(std::ranges::is_sorted(response.subscriber_nodes));
    CHECK_FALSE(outgoing.HasCommands<TopicInfoResponseCmd>());
  }

  TEST_CASE("bridge::TopicInfoNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicInfoNode node(incoming, outgoing);

    TopicInfoCmd query(std::pmr::get_default_resource());
    query.request_id = 42;
    query.topic_name = "/rosout";
    incoming.Enqueue(std::move(query));

    node.OnPollTimer();

    TopicInfoResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<TopicInfoResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 42);
    CHECK_EQ(response.topic_name, "/rosout");
  }
}
