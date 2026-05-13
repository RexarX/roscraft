#include <pch.hpp>

#include <roscraft/bridge/nodes/addon/event.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/addon.hpp>
#include <roscraft/bridge/command/types/error.hpp>

#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

AddonEventNode::AddonEventNode(CommandQueue& incoming, CommandQueue& outgoing,
                               std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_addon_event_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      incoming_consumer_(incoming.MakeConsumerToken<AddonEventCmd>()),
      outgoing_producer_(outgoing.MakeProducerToken<AddonEventCmd>()),
      error_producer_(outgoing.MakeProducerToken<ErrorCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;

  event_sub_ =
      this->create_subscription<roscraft_bridge_common::msg::AddonEvent>(
          "/roscraft/addon/event_in", rclcpp::QoS(10),
          [this](const roscraft_bridge_common::msg::AddonEvent::SharedPtr msg) {
            OnIncomingRosEvent(msg);
          });

  event_pub_ = this->create_publisher<roscraft_bridge_common::msg::AddonEvent>(
      "/roscraft/addon/event_out", rclcpp::QoS(10));

  drain_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void AddonEventNode::OnIncomingRosEvent(
    const roscraft_bridge_common::msg::AddonEvent::SharedPtr msg) {
  auto* const mr = std::pmr::get_default_resource();
  AddonEventCmd cmd(mr);
  cmd.request_id = msg->request_id;
  cmd.addon_id = msg->addon_id;
  cmd.event_type = msg->event_type;
  cmd.encoding = msg->encoding;
  cmd.response = msg->response;
  cmd.payload.assign_range(msg->payload);

  outgoing_.get().Enqueue(outgoing_producer_, std::move(cmd));
}

void AddonEventNode::OnPollTimer() {
  auto& storage = incoming_.get().TypedStorage<AddonEventCmd>();

  AddonEventCmd cmd(allocator_);
  roscraft_bridge_common::msg::AddonEvent ros_msg;
  while (storage.Dequeue(incoming_consumer_, cmd)) {
    ros_msg.request_id = cmd.request_id;
    ros_msg.addon_id = cmd.addon_id;
    ros_msg.event_type = cmd.event_type;
    ros_msg.encoding = cmd.encoding;
    ros_msg.response = cmd.response;
    ros_msg.payload.assign_range(cmd.payload);

    event_pub_->publish(ros_msg);
  }

  scratch_arena_.Reset();
}

void AddonEventNode::SendError(uint64_t request_id, std::string_view error_code,
                               std::string_view error_message) {
  ErrorCmd cmd(allocator_);
  cmd.request_id = request_id;
  cmd.error_code = error_code;
  cmd.error_message = error_message;

  outgoing_.get().Enqueue(error_producer_, std::move(cmd));
}

}  // namespace roscraft::bridge
