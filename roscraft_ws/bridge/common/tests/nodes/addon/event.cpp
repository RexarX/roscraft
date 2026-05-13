#include <doctest/doctest.h>

#include <sstream>

#define private public
#include <roscraft/bridge/nodes/addon/event.hpp>
#undef private

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/addon.hpp>
#include <roscraft/bridge/command/types/error.hpp>

#include <rclcpp/rclcpp.hpp>

#include <memory>
#include <memory_resource>
#include <string>

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
  incoming.Register<AddonEventCmd>();
  outgoing.Register<AddonEventCmd>();
  outgoing.Register<ErrorCmd>();
}

}  // namespace

TEST_SUITE("bridge::AddonEventNode") {
  TEST_CASE("bridge::AddonEventNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    AddonEventNode node(incoming, outgoing, std::pmr::get_default_resource());

    CHECK(std::string_view(node.get_name()) == "roscraft_addon_event_node");
    CHECK_NE(node.event_sub_, nullptr);
    CHECK_NE(node.event_pub_, nullptr);
    CHECK_NE(node.drain_timer_, nullptr);
  }

  TEST_CASE("bridge::AddonEventNode::OnIncomingRosEvent enqueues to outgoing") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    AddonEventNode node(incoming, outgoing, std::pmr::get_default_resource());

    auto msg = std::make_shared<roscraft_bridge_common::msg::AddonEvent>();
    msg->request_id = 42U;
    msg->addon_id = "ping";
    msg->event_type = "hello";
    msg->encoding = "utf-8";
    msg->response = false;
    msg->payload = {0x01, 0x02};

    node.OnIncomingRosEvent(msg);

    AddonEventCmd cmd{};
    CHECK(outgoing.TypedStorage<AddonEventCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 42U);
    CHECK_EQ(cmd.addon_id, "ping");
    CHECK_EQ(cmd.event_type, "hello");
    CHECK_EQ(cmd.encoding, "utf-8");
    CHECK_FALSE(cmd.response);
    CHECK_EQ(cmd.payload.size(), 2U);
    CHECK_EQ(cmd.payload[0], 0x01);
    CHECK_EQ(cmd.payload[1], 0x02);
  }

  TEST_CASE("bridge::AddonEventNode::OnIncomingRosEvent with empty fields") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    AddonEventNode node(incoming, outgoing, std::pmr::get_default_resource());

    auto msg = std::make_shared<roscraft_bridge_common::msg::AddonEvent>();
    msg->request_id = 0U;
    msg->addon_id = "";
    msg->event_type = "";
    msg->encoding = "";
    msg->response = true;
    // payload empty

    node.OnIncomingRosEvent(msg);

    AddonEventCmd cmd{};
    CHECK(outgoing.TypedStorage<AddonEventCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 0U);
    CHECK(cmd.addon_id.empty());
    CHECK(cmd.event_type.empty());
    CHECK(cmd.encoding.empty());
    CHECK(cmd.response);
    CHECK(cmd.payload.empty());
  }

  TEST_CASE("bridge::AddonEventNode::OnPollTimer drains incoming to ROS") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    AddonEventNode node(incoming, outgoing, std::pmr::get_default_resource());

    AddonEventCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 99U;
    cmd.addon_id = "echo";
    cmd.event_type = "pong";
    cmd.encoding = "json";
    cmd.response = true;
    cmd.payload = {0xAA, 0xBB, 0xCC};
    incoming.Enqueue(std::move(cmd));

    node.OnPollTimer();

    INFO("After OnPollTimer, incoming queue should be drained");
    CHECK_EQ(incoming.CommandCount<AddonEventCmd>(), 0U);
  }

  TEST_CASE("bridge::AddonEventNode::SendError") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    AddonEventNode node(incoming, outgoing, std::pmr::get_default_resource());

    node.SendError(123U, "CODE", "message");

    ErrorCmd cmd;
    CHECK(outgoing.TypedStorage<ErrorCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 123U);
    CHECK_EQ(cmd.error_code, "CODE");
    CHECK_EQ(cmd.error_message, "message");
  }
}
