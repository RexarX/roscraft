#include <doctest/doctest.h>

#include <sstream>

#define private public
#include <roscraft/bridge/nodes/ros/interface/list.hpp>
#undef private

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/interface.hpp>

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
  incoming.Register<InterfaceListCmd>();
  outgoing.Register<InterfaceListResponseCmd>();
}

}  // namespace

TEST_SUITE("bridge::InterfaceListNode") {
  TEST_CASE("bridge::InterfaceListNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    InterfaceListNode node(incoming, outgoing,
                           std::pmr::get_default_resource());

    CHECK(std::string_view(node.get_name()) == "roscraft_interface_list_node");
    CHECK_NE(node.poll_timer_, nullptr);
  }

  TEST_CASE("bridge::InterfaceListNode::DrainInterfaceListCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    InterfaceListNode node(incoming, outgoing,
                           std::pmr::get_default_resource());

    InterfaceListCmd query{
        .request_id = 63,
        .include_messages = true,
        .include_services = true,
        .include_actions = false,
    };
    incoming.Enqueue(std::move(query));

    node.DrainInterfaceListCommands();

    InterfaceListResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<InterfaceListResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 63);
    CHECK_FALSE(response.messages.empty());
    CHECK_FALSE(response.services.empty());
    CHECK(response.actions.empty());
    CHECK_FALSE(outgoing.HasCommands<InterfaceListResponseCmd>());
  }

  TEST_CASE("bridge::InterfaceListNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    InterfaceListNode node(incoming, outgoing,
                           std::pmr::get_default_resource());

    InterfaceListCmd list_query{
        .request_id = 64,
        .include_messages = true,
        .include_services = false,
        .include_actions = false,
    };
    incoming.Enqueue(std::move(list_query));

    node.OnPollTimer();

    InterfaceListResponseCmd list_response(std::pmr::get_default_resource());

    CHECK(outgoing.TypedStorage<InterfaceListResponseCmd>().Dequeue(
        list_response));
    CHECK_EQ(list_response.request_id, 64);
  }
}
