#include <doctest/doctest.h>

#include <sstream>

#define private public
#include <roscraft/bridge/nodes/ros/interface/show.hpp>
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
  incoming.Register<InterfaceShowCmd>();
  outgoing.Register<InterfaceShowResponseCmd>();
}

}  // namespace

TEST_SUITE("bridge::InterfaceShowNode") {
  TEST_CASE("bridge::InterfaceShowNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    InterfaceShowNode node(incoming, outgoing,
                           std::pmr::get_default_resource());

    CHECK(std::string_view(node.get_name()) == "roscraft_interface_show_node");
    CHECK_NE(node.poll_timer_, nullptr);
  }

  TEST_CASE("bridge::InterfaceShowNode::DrainInterfaceShowCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    InterfaceShowNode node(incoming, outgoing,
                           std::pmr::get_default_resource());

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

  TEST_CASE("bridge::InterfaceShowNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    InterfaceShowNode node(incoming, outgoing,
                           std::pmr::get_default_resource());

    InterfaceShowCmd show_query(std::pmr::get_default_resource());
    show_query.request_id = 65;
    show_query.interface_type = "std_msgs/msg/String";
    incoming.Enqueue(std::move(show_query));

    node.OnPollTimer();

    InterfaceShowResponseCmd show_response(std::pmr::get_default_resource());

    CHECK(outgoing.TypedStorage<InterfaceShowResponseCmd>().Dequeue(
        show_response));
    CHECK_EQ(show_response.request_id, 65);
  }
}
