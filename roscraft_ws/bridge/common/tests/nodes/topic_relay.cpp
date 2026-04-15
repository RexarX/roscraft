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
  incoming.Register<PublishMessageCmd>();
  outgoing.Register<TopicPayloadCmd>();
  outgoing.Register<ErrorCmd>();
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
    CHECK_EQ(node.subscriptions_.size(), 0);
    CHECK_EQ(node.publishers_.size(), 0);
  }

  TEST_CASE("bridge::TopicRelayNode::DrainSubscribeCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing);

    SubscribeTopicCmd command(std::pmr::get_default_resource());
    command.request_id = 7;
    command.topic_name = "/topic_relay_test";
    command.message_type = "std_msgs/msg/String";
    command.once = true;
    command.timeout_seconds = 1.25;
    command.raw = true;

    incoming.Enqueue(std::move(command));

    node.DrainSubscribeCommands();

    CHECK(node.subscriptions_.contains("/topic_relay_test"));
    CHECK_EQ(node.subscriptions_.size(), 1);
    const auto& state = node.subscriptions_.at("/topic_relay_test");
    REQUIRE_EQ(state.requests.size(), 1);
    CHECK_EQ(state.requests.front().request_id, 7);
    CHECK(state.requests.front().once);
    CHECK(state.requests.front().raw);
    CHECK(node.once_topics_.contains(7));
    CHECK(node.timeout_timers_.contains(7));
  }

  TEST_CASE("bridge::TopicRelayNode::DrainPublishCommands type mismatch") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing);

    node.publishers_.emplace("/topic_relay_publish",
                             std::shared_ptr<rclcpp::GenericPublisher>{});
    node.publisher_types_.emplace("/topic_relay_publish",
                                  "std_msgs/msg/String");

    PublishMessageCmd publish(std::pmr::get_default_resource());
    publish.request_id = 88;
    publish.topic_name = "/topic_relay_publish";
    publish.message_type = "std_msgs/msg/Bool";
    publish.payload.assign({0, 1, 2, 3});

    incoming.Enqueue(std::move(publish));
    node.DrainPublishCommands();

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 88);
    CHECK_EQ(err.error_code, "PUBLISH_FAILED");
    CHECK_FALSE(err.error_message.empty());
  }

  TEST_CASE("bridge::TopicRelayNode::DrainPublishCommands invalid input") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing);

    PublishMessageCmd publish(std::pmr::get_default_resource());
    publish.request_id = 89;
    publish.topic_name = "";
    publish.message_type = "std_msgs/msg/String";

    incoming.Enqueue(std::move(publish));
    node.DrainPublishCommands();

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 89);
    CHECK_EQ(err.error_code, "PUBLISH_FAILED");
    CHECK_FALSE(err.error_message.empty());
  }

  TEST_CASE(
      "bridge::TopicRelayNode::Subscribe with invalid type sends ErrorCmd") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing);

    SubscribeTopicCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 42;
    cmd.topic_name = "/bad_topic";
    cmd.message_type = "invalid_type";
    node.Subscribe(cmd);

    CHECK_FALSE(node.subscriptions_.contains("/bad_topic"));

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 42);
    CHECK_EQ(err.error_code, "SUBSCRIBE_FAILED");
    CHECK_FALSE(err.error_message.empty());
  }

  TEST_CASE("bridge::TopicRelayNode::Subscribe duplicate request id ignored") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing);

    SubscribeTopicCmd first(std::pmr::get_default_resource());
    first.request_id = 1;
    first.topic_name = "/dup_topic";
    first.message_type = "std_msgs/msg/String";
    node.Subscribe(first);

    SubscribeTopicCmd second(std::pmr::get_default_resource());
    second.request_id = 1;
    second.topic_name = "/dup_topic";
    second.message_type = "std_msgs/msg/String";
    node.Subscribe(second);

    REQUIRE(node.subscriptions_.contains("/dup_topic"));
    CHECK_EQ(node.subscriptions_.at("/dup_topic").requests.size(), 1);

    ErrorCmd err;
    CHECK_FALSE(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
  }

  TEST_CASE("bridge::TopicRelayNode::RemoveTopicRequest") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing);

    SubscribeTopicCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 55;
    cmd.topic_name = "/remove_topic";
    cmd.message_type = "std_msgs/msg/String";
    cmd.once = true;
    cmd.timeout_seconds = 1.0;
    node.Subscribe(cmd);

    CHECK(node.subscriptions_.contains("/remove_topic"));
    CHECK(node.once_topics_.contains(55));
    CHECK(node.timeout_timers_.contains(55));

    node.RemoveTopicRequest("/remove_topic", 55);

    CHECK_FALSE(node.subscriptions_.contains("/remove_topic"));
    CHECK_FALSE(node.once_topics_.contains(55));
    CHECK_FALSE(node.timeout_timers_.contains(55));
  }

  TEST_CASE("bridge::TopicRelayNode::ClearTimeout") {
    using namespace std::chrono_literals;

    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing);

    node.timeout_timers_[91] = node.create_wall_timer(10ms, [] {});
    CHECK(node.timeout_timers_.contains(91));

    node.ClearTimeout(91);

    CHECK_FALSE(node.timeout_timers_.contains(91));
  }

  TEST_CASE("bridge::TopicRelayNode::EnsurePublisher type mismatch") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing);

    node.publishers_.emplace("/topic_pub",
                             std::shared_ptr<rclcpp::GenericPublisher>{});
    node.publisher_types_.emplace("/topic_pub", "std_msgs/msg/String");
    const auto second =
        node.EnsurePublisher("/topic_pub", 11, "std_msgs/msg/Bool");
    CHECK_FALSE(second.has_value());

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 11);
    CHECK_EQ(err.error_code, "PUBLISH_FAILED");
  }
}
