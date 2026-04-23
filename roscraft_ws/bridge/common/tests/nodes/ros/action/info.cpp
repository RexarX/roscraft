#include <doctest/doctest.h>

#include <sstream>

#define private public
#include <roscraft/bridge/nodes/ros/action/info.hpp>
#undef private

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/action.hpp>

#include <rclcpp/rclcpp.hpp>

#include <memory_resource>
#include <string_view>

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
  incoming.Register<ActionInfoCmd>();
  outgoing.Register<ActionInfoResponseCmd>();
}

}  // namespace

TEST_SUITE("bridge::ActionInfoNode") {
  TEST_CASE("bridge::ActionInfoNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    ActionInfoNode node(incoming, outgoing, std::pmr::get_default_resource());

    CHECK(std::string_view(node.get_name()) == "roscraft_action_info_node");
    CHECK_NE(node.poll_timer_, nullptr);
  }

  TEST_CASE("bridge::ActionInfoNode::DrainActionInfoCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ActionInfoNode node(incoming, outgoing, std::pmr::get_default_resource());

    ActionInfoCmd query(std::pmr::get_default_resource());
    query.request_id = 301;
    query.action_name = "/fibonacci";
    query.include_hidden = true;
    incoming.Enqueue(std::move(query));

    node.DrainActionInfoCommands();

    ActionInfoResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<ActionInfoResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 301);
    CHECK_EQ(response.action_name, "/fibonacci");
    CHECK_GE(response.client_count, 0U);
    CHECK_GE(response.server_count, 0U);
    CHECK_GE(response.feedback_publisher_count, 0U);
    CHECK_GE(response.feedback_subscriber_count, 0U);
    CHECK_GE(response.status_publisher_count, 0U);
    CHECK_GE(response.status_subscriber_count, 0U);
  }

  TEST_CASE("bridge::ActionInfoNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ActionInfoNode node(incoming, outgoing, std::pmr::get_default_resource());

    ActionInfoCmd query(std::pmr::get_default_resource());
    query.request_id = 302;
    query.action_name = "/fibonacci";
    query.include_hidden = false;
    incoming.Enqueue(std::move(query));

    node.OnPollTimer();

    ActionInfoResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<ActionInfoResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 302);
    CHECK_EQ(response.action_name, "/fibonacci");
  }
}
