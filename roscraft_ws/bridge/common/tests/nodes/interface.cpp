#include <doctest/doctest.h>

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/rclcpp.hpp>

#include <memory_resource>

#define private public
#include <roscraft/bridge/nodes/interface.hpp>
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
  incoming.Register<InterfaceListCmd>();
  incoming.Register<InterfaceShowCmd>();
  outgoing.Register<InterfaceListResponseCmd>();
  outgoing.Register<InterfaceShowResponseCmd>();
}

}  // namespace

TEST_SUITE("bridge::InterfaceNode") {
  TEST_CASE("bridge::InterfaceNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    InterfaceNode node(incoming, outgoing);

    CHECK_EQ(std::string_view(node.get_name()), "roscraft_interface_node");
    CHECK_NE(node.poll_timer_, nullptr);
  }

  TEST_CASE("bridge::InterfaceNode::DrainInterfaceShowCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    InterfaceNode node(incoming, outgoing);

    InterfaceShowCmd query(std::pmr::get_default_resource());
    query.request_id = 61;
    query.interface_type = "std_msgs/msg/String";
    incoming.Enqueue(std::move(query));

    node.DrainInterfaceShowCommands();

    InterfaceShowResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<InterfaceShowResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 61);
    CHECK_EQ(response.interface_type, "std_msgs/msg/String");
    CHECK(response.found);
    CHECK_FALSE(response.definition.empty());
    CHECK_FALSE(outgoing.HasCommands<InterfaceShowResponseCmd>());
  }

  TEST_CASE("bridge::InterfaceNode::DrainInterfaceListCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    InterfaceNode node(incoming, outgoing);

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

  TEST_CASE("bridge::InterfaceNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    InterfaceNode node(incoming, outgoing);

    InterfaceListCmd list_query{
        .request_id = 64,
        .include_messages = true,
        .include_services = false,
        .include_actions = false,
    };
    incoming.Enqueue(std::move(list_query));

    InterfaceShowCmd show_query(std::pmr::get_default_resource());
    show_query.request_id = 65;
    show_query.interface_type = "std_msgs/msg/String";
    incoming.Enqueue(std::move(show_query));

    node.OnPollTimer();

    InterfaceListResponseCmd list_response(std::pmr::get_default_resource());
    InterfaceShowResponseCmd show_response(std::pmr::get_default_resource());

    CHECK(outgoing.TypedStorage<InterfaceListResponseCmd>().Dequeue(
        list_response));
    CHECK(outgoing.TypedStorage<InterfaceShowResponseCmd>().Dequeue(
        show_response));
    CHECK_EQ(list_response.request_id, 64);
    CHECK_EQ(show_response.request_id, 65);
  }
}
