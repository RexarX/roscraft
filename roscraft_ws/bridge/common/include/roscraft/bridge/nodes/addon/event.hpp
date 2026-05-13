#pragma once

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/memory/arena_allocator.hpp>

#include <rclcpp/node.hpp>
#include <rclcpp/publisher.hpp>
#include <rclcpp/subscription.hpp>
#include <rclcpp/timer.hpp>

#include <roscraft_bridge_common/msg/addon_event.hpp>

#include <cstdint>
#include <functional>
#include <memory_resource>
#include <string_view>

namespace roscraft::bridge {

class AddonEventNode final : public rclcpp::Node {
public:
  AddonEventNode(
      CommandQueue& incoming, CommandQueue& outgoing,
      std::pmr::memory_resource* allocator = std::pmr::get_default_resource());
  AddonEventNode(const AddonEventNode&) = delete;
  AddonEventNode(AddonEventNode&&) = delete;
  ~AddonEventNode() override = default;

  AddonEventNode& operator=(const AddonEventNode&) = delete;
  AddonEventNode& operator=(AddonEventNode&&) = delete;

private:
  void OnIncomingRosEvent(
      const roscraft_bridge_common::msg::AddonEvent::SharedPtr msg);
  void OnPollTimer();
  void SendError(uint64_t request_id, std::string_view error_code,
                 std::string_view error_message);

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken incoming_consumer_;
  CommandQueueProducerToken outgoing_producer_;
  CommandQueueProducerToken error_producer_;

  rclcpp::Subscription<roscraft_bridge_common::msg::AddonEvent>::SharedPtr
      event_sub_;
  rclcpp::Publisher<roscraft_bridge_common::msg::AddonEvent>::SharedPtr
      event_pub_;

  rclcpp::TimerBase::SharedPtr drain_timer_;

  std::pmr::memory_resource* allocator_ = std::pmr::get_default_resource();
  memory::ArenaAllocator scratch_arena_{128 * 1024};
};

}  // namespace roscraft::bridge
