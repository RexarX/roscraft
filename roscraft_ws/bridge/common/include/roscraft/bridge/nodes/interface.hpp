#pragma once

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <functional>

namespace roscraft::bridge {

/// @brief Serves `InterfaceListCmd` and `InterfaceShowCmd` requests.
class InterfaceNode final : public rclcpp::Node {
public:
  /// @brief Construct interface service node.
  /// @param incoming Incoming command queue
  /// @param outgoing Outgoing command queue
  InterfaceNode(CommandQueue& incoming, CommandQueue& outgoing);
  InterfaceNode(const InterfaceNode&) = delete;
  InterfaceNode(InterfaceNode&&) = delete;
  ~InterfaceNode() override = default;

  InterfaceNode& operator=(const InterfaceNode&) = delete;
  InterfaceNode& operator=(InterfaceNode&&) = delete;

private:
  /// @brief Drain pending `InterfaceListCmd` commands.
  void DrainInterfaceListCommands();

  /// @brief Drain pending `InterfaceShowCmd` commands.
  void DrainInterfaceShowCommands();

  /// @brief Periodic callback.
  void OnPollTimer();

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken interface_list_consumer_;
  CommandQueueConsumerToken interface_show_consumer_;
  CommandQueueProducerToken interface_list_response_producer_;
  CommandQueueProducerToken interface_show_response_producer_;

  rclcpp::TimerBase::SharedPtr poll_timer_;
};

}  // namespace roscraft::bridge
