#include <doctest/doctest.h>

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/rclcpp.hpp>

#include <algorithm>
#include <memory_resource>
#include <ranges>

#define private public
#include <roscraft/bridge/nodes/service_info.hpp>
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
  incoming.Register<ServiceInfoCmd>();
  outgoing.Register<ServiceInfoResponseCmd>();
}

}  // namespace

TEST_SUITE("bridge::ServiceInfoNode") {
  TEST_CASE("bridge::ServiceInfoNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    ServiceInfoNode node(incoming, outgoing);

    CHECK_EQ(std::string_view(node.get_name()), "roscraft_service_info_node");
    CHECK_NE(node.poll_timer_, nullptr);
  }

  TEST_CASE("bridge::ServiceInfoNode::DrainServiceInfoCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ServiceInfoNode node(incoming, outgoing);

    ServiceInfoCmd query(std::pmr::get_default_resource());
    query.request_id = 51;
    query.service_name = "/roscraft_service_info_node/get_parameters";
    incoming.Enqueue(std::move(query));

    node.DrainServiceInfoCommands();

    ServiceInfoResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<ServiceInfoResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 51);
    CHECK_EQ(response.service_name,
             "/roscraft_service_info_node/get_parameters");
    CHECK_GE(response.client_count, 0);
    CHECK_GE(response.server_count, 0);
    CHECK_LE(response.client_nodes.size(), response.client_count);
    CHECK_LE(response.server_nodes.size(), response.server_count);
    CHECK(std::ranges::is_sorted(response.client_nodes));
    CHECK(std::ranges::is_sorted(response.server_nodes));
    CHECK_FALSE(outgoing.HasCommands<ServiceInfoResponseCmd>());
  }

  TEST_CASE("bridge::ServiceInfoNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ServiceInfoNode node(incoming, outgoing);

    ServiceInfoCmd query(std::pmr::get_default_resource());
    query.request_id = 52;
    query.service_name = "/roscraft_service_info_node/get_parameters";
    incoming.Enqueue(std::move(query));

    node.OnPollTimer();

    ServiceInfoResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<ServiceInfoResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 52);
    CHECK_EQ(response.service_name,
             "/roscraft_service_info_node/get_parameters");
  }
}
