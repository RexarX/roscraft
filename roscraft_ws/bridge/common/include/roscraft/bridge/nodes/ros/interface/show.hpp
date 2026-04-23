#pragma once

#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <functional>
#include <memory_resource>

namespace roscraft::bridge {

/// @brief Serves `InterfaceListCmd` and `InterfaceShowCmd` requests.
class InterfaceShowNode final : public rclcpp::Node {
public:
  /// @brief Construct interface service node.
  /// @param incoming Incoming command queue
  /// @param outgoing Outgoing command queue
  /// @param allocator The memory resource for command allocation (default:
  /// `std::pmr::get_default_resource()`)
  InterfaceShowNode(
      CommandQueue& incoming, CommandQueue& outgoing,
      std::pmr::memory_resource* allocator = std::pmr::get_default_resource());

  InterfaceShowNode(CommandQueue& incoming, CommandQueue& outgoing,
                    std::nullptr_t) = delete;

  InterfaceShowNode(const InterfaceShowNode&) = delete;
  InterfaceShowNode(InterfaceShowNode&&) = delete;
  ~InterfaceShowNode() override = default;

  InterfaceShowNode& operator=(const InterfaceShowNode&) = delete;
  InterfaceShowNode& operator=(InterfaceShowNode&&) = delete;

private:
  /// @brief Drain pending `InterfaceShowCmd` commands.
  void DrainInterfaceShowCommands();

  /// @brief Periodic callback.
  void OnPollTimer();

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken interface_show_consumer_;
  CommandQueueProducerToken interface_show_response_producer_;

  rclcpp::TimerBase::SharedPtr poll_timer_;

  std::pmr::memory_resource* allocator_ = std::pmr::get_default_resource();
};

}  // namespace roscraft::bridge
