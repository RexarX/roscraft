#pragma once

#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <functional>
#include <memory_resource>

namespace roscraft::bridge {

/// @brief Serves `InterfaceListCmd` requests.
class InterfaceListNode final : public rclcpp::Node {
public:
  /// @brief Construct interface service node.
  /// @param incoming Incoming command queue
  /// @param outgoing Outgoing command queue
  /// @param allocator The memory resource for command allocation (default:
  /// `std::pmr::get_default_resource()`)
  InterfaceListNode(CommandQueue& incoming, CommandQueue& outgoing,
                    std::pmr::memory_resource* allocator);

  InterfaceListNode(CommandQueue& incoming, CommandQueue& outgoing,
                    std::nullptr_t) = delete;

  InterfaceListNode(const InterfaceListNode&) = delete;
  InterfaceListNode(InterfaceListNode&&) = delete;
  ~InterfaceListNode() override = default;

  InterfaceListNode& operator=(const InterfaceListNode&) = delete;
  InterfaceListNode& operator=(InterfaceListNode&&) = delete;

private:
  /// @brief Drain pending `InterfaceListCmd` commands.
  void DrainInterfaceListCommands();

  /// @brief Periodic callback.
  void OnPollTimer();

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken interface_list_consumer_;
  CommandQueueProducerToken interface_list_response_producer_;

  rclcpp::TimerBase::SharedPtr poll_timer_;

  std::pmr::memory_resource* allocator_ = std::pmr::get_default_resource();
};

}  // namespace roscraft::bridge
