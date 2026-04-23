#include <doctest/doctest.h>

#include <sstream>

#define private public
#include <roscraft/bridge/nodes/ros/node/info.hpp>
#undef private

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/node.hpp>

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
  incoming.Register<NodeInfoCmd>();
  outgoing.Register<NodeInfoResponseCmd>();
}

}  // namespace

TEST_SUITE("bridge::NodeInfoNode") {
  TEST_CASE("bridge::NodeInfoNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    NodeInfoNode node(incoming, outgoing, std::pmr::get_default_resource());

    CHECK(std::string_view(node.get_name()) == "roscraft_node_info_node");
    CHECK_NE(node.poll_timer_, nullptr);
  }

  TEST_CASE("bridge::NodeInfoNode::DrainNodeInfoCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    NodeInfoNode node(incoming, outgoing, std::pmr::get_default_resource());

    NodeInfoCmd query(std::pmr::get_default_resource());
    query.request_id = 45;
    query.node_name = "/roscraft_node_info_node";
    query.include_hidden = true;
    incoming.Enqueue(std::move(query));

    node.DrainNodeInfoCommands();

    NodeInfoResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<NodeInfoResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 45);
    CHECK_EQ(response.node_name, "/roscraft_node_info_node");
    CHECK(response.found);
    CHECK_FALSE(outgoing.HasCommands<NodeInfoResponseCmd>());
  }

  TEST_CASE("bridge::NodeInfoNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    NodeInfoNode node(incoming, outgoing, std::pmr::get_default_resource());

    NodeInfoCmd query(std::pmr::get_default_resource());
    query.request_id = 46;
    query.node_name = "/roscraft_node_info_node";
    query.include_hidden = false;
    incoming.Enqueue(std::move(query));

    node.OnPollTimer();

    NodeInfoResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<NodeInfoResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 46);
    CHECK_EQ(response.node_name, "/roscraft_node_info_node");
  }
}
