#include <doctest/doctest.h>

#include <sstream>

#define private public
#include <roscraft/bridge/nodes/ros/topic/relay.hpp>
#undef private

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/error.hpp>
#include <roscraft/bridge/command/types/topic.hpp>

#include <rclcpp/rclcpp.hpp>

#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

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
  incoming.Register<TopicSubscribeCmd>();
  incoming.Register<TopicUnsubscribeCmd>();
  incoming.Register<TopicPublishMessageCmd>();
  outgoing.Register<TopicPayloadCmd>();
  outgoing.Register<ErrorCmd>();
}

[[nodiscard]] auto ToPayload(std::string_view text) -> std::vector<uint8_t> {
  return {text.begin(), text.end()};
}

}  // namespace

TEST_SUITE("bridge::TopicRelayNode") {
  TEST_CASE("bridge::TopicRelayNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    CHECK(std::string_view(node.get_name()) == "roscraft_topic_relay_node");
    CHECK_NE(node.drain_timer_, nullptr);
    CHECK_EQ(node.subscriptions_.size(), 0);
    CHECK_EQ(node.publishers_.size(), 0);
  }

  TEST_CASE("bridge::TopicRelayNode::DrainSubscribeCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicSubscribeCmd command(std::pmr::get_default_resource());
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
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    node.publishers_.emplace("/topic_relay_publish",
                             std::shared_ptr<rclcpp::GenericPublisher>{});
    node.publisher_types_.emplace("/topic_relay_publish",
                                  "std_msgs/msg/String");

    TopicPublishMessageCmd publish(std::pmr::get_default_resource());
    publish.request_id = 88;
    publish.topic_name = "/topic_relay_publish";
    publish.message_type = "std_msgs/msg/Bool";
    const auto payload = ToPayload("data: true\n");
    publish.payload.assign(payload.begin(), payload.end());
    publish.qos_profile = "default";

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
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicPublishMessageCmd publish(std::pmr::get_default_resource());
    publish.request_id = 89;
    publish.topic_name = "";
    publish.message_type = "std_msgs/msg/String";
    publish.qos_profile = "default";

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
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicSubscribeCmd cmd(std::pmr::get_default_resource());
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
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicSubscribeCmd first(std::pmr::get_default_resource());
    first.request_id = 1;
    first.topic_name = "/dup_topic";
    first.message_type = "std_msgs/msg/String";
    node.Subscribe(first);

    TopicSubscribeCmd second(std::pmr::get_default_resource());
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
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicSubscribeCmd cmd(std::pmr::get_default_resource());
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
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

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
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    node.publishers_.emplace("/topic_pub",
                             std::shared_ptr<rclcpp::GenericPublisher>{});
    node.publisher_types_.emplace("/topic_pub", "std_msgs/msg/String");
    node.publisher_qos_profiles_.emplace("/topic_pub", "default");
    const auto second =
        node.EnsurePublisher("/topic_pub", 11, "std_msgs/msg/Bool", "default");
    CHECK_FALSE(second.has_value());

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 11);
    CHECK_EQ(err.error_code, "PUBLISH_FAILED");
  }

  TEST_CASE("bridge::TopicRelayNode::EnsureMessageIntrospection") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    const auto* first = node.EnsureMessageIntrospection("std_msgs/msg/String");
    REQUIRE_NE(first, nullptr);
    REQUIRE(node.message_introspection_cache_.contains("std_msgs/msg/String"));
    CHECK(node.message_introspection_cache_.at("std_msgs/msg/String")
              .has_value());

    const auto* second = node.EnsureMessageIntrospection("std_msgs/msg/String");
    REQUIRE_NE(second, nullptr);
    CHECK_EQ(second, first);

    const auto* invalid = node.EnsureMessageIntrospection("invalid/type");
    CHECK_EQ(invalid, nullptr);
    REQUIRE(node.message_introspection_cache_.contains("invalid/type"));
    CHECK_FALSE(
        node.message_introspection_cache_.at("invalid/type").has_value());
  }

  TEST_CASE("bridge::TopicRelayNode::SerializeYamlPayload") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    const auto valid_payload = ToPayload("data: hello\n");
    const auto serialized = node.SerializeYamlPayload(
        301, "/topic_yaml", "std_msgs/msg/String",
        std::span<const uint8_t>(valid_payload.data(), valid_payload.size()));
    REQUIRE(serialized.has_value());
    CHECK_FALSE(serialized->empty());

    const auto invalid_payload = ToPayload("value: nope\n");
    const auto invalid = node.SerializeYamlPayload(
        302, "/topic_yaml", "std_msgs/msg/String",
        std::span<const uint8_t>(invalid_payload.data(),
                                 invalid_payload.size()));
    CHECK_FALSE(invalid.has_value());

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 302);
    CHECK_EQ(err.error_code, "PUBLISH_FAILED");
    CHECK_FALSE(err.error_message.empty());
  }

  TEST_CASE("bridge::TopicRelayNode::Publish repeated stream creates timer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicPublishMessageCmd publish(std::pmr::get_default_resource());
    publish.request_id = 101;
    publish.topic_name = "/topic_relay_repeat";
    publish.message_type = "std_msgs/msg/String";
    const auto payload = ToPayload("data: repeated\n");
    publish.payload.assign(payload.begin(), payload.end());
    publish.times = 3;
    publish.rate_hz = 20.0;
    publish.qos_profile = "default";

    node.Publish(publish);

    CHECK(node.publish_timers_.contains(101));
  }

  TEST_CASE("bridge::TopicRelayNode::Publish invalid qos profile") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicPublishMessageCmd publish(std::pmr::get_default_resource());
    publish.request_id = 102;
    publish.topic_name = "/topic_relay_invalid_qos";
    publish.message_type = "std_msgs/msg/String";
    const auto payload = ToPayload("data: qos\n");
    publish.payload.assign(payload.begin(), payload.end());
    publish.qos_profile = "unknown_profile";

    node.Publish(publish);

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 102);
    CHECK_EQ(err.error_code, "PUBLISH_FAILED");
    CHECK_FALSE(err.error_message.empty());
  }

  TEST_CASE("bridge::TopicRelayNode::Publish invalid rate") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicPublishMessageCmd publish(std::pmr::get_default_resource());
    publish.request_id = 103;
    publish.topic_name = "/topic_relay_invalid_rate";
    publish.message_type = "std_msgs/msg/String";
    const auto payload = ToPayload("data: rate\n");
    publish.payload.assign(payload.begin(), payload.end());
    publish.rate_hz = -1.0;

    node.Publish(publish);

    ErrorCmd err;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(err));
    CHECK_EQ(err.request_id, 103);
    CHECK_EQ(err.error_code, "PUBLISH_FAILED");
  }

  TEST_CASE("bridge::TopicRelayNode::RemoveTopicRequest keeps shared topic") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    TopicRelayNode node(incoming, outgoing, std::pmr::get_default_resource());

    TopicSubscribeCmd first(std::pmr::get_default_resource());
    first.request_id = 201;
    first.topic_name = "/topic_relay_shared";
    first.message_type = "std_msgs/msg/String";
    node.Subscribe(first);

    TopicSubscribeCmd second(std::pmr::get_default_resource());
    second.request_id = 202;
    second.topic_name = "/topic_relay_shared";
    second.message_type = "std_msgs/msg/String";
    node.Subscribe(second);

    REQUIRE(node.subscriptions_.contains("/topic_relay_shared"));
    REQUIRE_EQ(node.subscriptions_.at("/topic_relay_shared").requests.size(),
               2);

    node.RemoveTopicRequest("/topic_relay_shared", 201);

    CHECK(node.subscriptions_.contains("/topic_relay_shared"));
    REQUIRE_EQ(node.subscriptions_.at("/topic_relay_shared").requests.size(),
               1);
    CHECK_EQ(node.subscriptions_.at("/topic_relay_shared")
                 .requests.front()
                 .request_id,
             202);
  }
}
