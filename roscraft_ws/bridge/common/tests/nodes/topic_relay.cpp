#include <doctest/doctest.h>

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/rclcpp.hpp>

#include <memory>
#include <memory_resource>
#include <string>

#define private public
#include <roscraft/bridge/nodes/topic_relay.hpp>
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
  incoming.Register<SubscribeTopicCmd>();
  outgoing.Register<TopicPayloadCmd>();
}

}  // namespace

TEST_SUITE("bridge::TopicRelayNode") {
  TEST_CASE("bridge::TopicRelayNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    TopicRelayNode node(incoming, outgoing);

    CHECK_EQ(std::string_view(node.get_name()), "roscraft_topic_relay_node");
    CHECK_NE(node.drain_timer_, nullptr);
    CHECK_EQ(node.subscriptions_.size(), 0U);
  }

  TEST_CASE("bridge::TopicRelayNode::~TopicRelayNode") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    {
      TopicRelayNode node(incoming, outgoing);
    }

    CHECK(true);
  }

  TEST_CASE("bridge::TopicRelayNode::DrainSubscribeCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing);

    SubscribeTopicCmd command(std::pmr::get_default_resource());
    command.request_id = 7U;
    command.topic_name = "/topic_relay_test";
    command.message_type = "std_msgs/msg/String";

    incoming.Enqueue(std::move(command));

    node.DrainSubscribeCommands();

    CHECK(node.subscriptions_.contains("/topic_relay_test"));
    CHECK_EQ(node.subscriptions_.size(), 1U);
  }

  TEST_CASE("bridge::TopicRelayNode::Subscribe") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing);

    node.Subscribe("/topic_relay_subscribe", "std_msgs/msg/String");

    CHECK(node.subscriptions_.contains("/topic_relay_subscribe"));
    CHECK_EQ(node.subscriptions_.size(), 1U);
  }
}
