#pragma once

#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <cstdint>
#include <functional>
#include <memory_resource>
#include <string_view>

namespace roscraft::bridge {

/// @brief Serves `param` command family requests.
class ParamListNode final : public rclcpp::Node {
public:
  /// @brief Construct param node.
  /// @param incoming Incoming command queue
  /// @param outgoing Outgoing command queue
  /// @param allocator The memory resource for command allocation (default:
  /// `std::pmr::get_default_resource()`)
  ParamListNode(
      CommandQueue& incoming, CommandQueue& outgoing,
      std::pmr::memory_resource* allocator = std::pmr::get_default_resource());

  ParamListNode(CommandQueue& incoming, CommandQueue& outgoing,
                std::nullptr_t) = delete;

  ParamListNode(const ParamListNode&) = delete;
  ParamListNode(ParamListNode&&) = delete;
  ~ParamListNode() override = default;

  ParamListNode& operator=(const ParamListNode&) = delete;
  ParamListNode& operator=(ParamListNode&&) = delete;

private:
  /// @brief Drain pending `ParamListCmd` commands.
  void DrainParamListCommands();

  /// @brief Periodic callback.
  void OnPollTimer();

  /// @brief Enqueue an `ErrorCmd` to the outgoing queue.
  void SendError(uint64_t request_id, std::string_view error_code,
                 std::string_view error_message);

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken param_list_consumer_;
  CommandQueueProducerToken param_list_response_producer_;
  CommandQueueProducerToken error_producer_;

  rclcpp::TimerBase::SharedPtr poll_timer_;

  rclcpp::Node::SharedPtr temp_node_;

  std::pmr::memory_resource* allocator_ = std::pmr::get_default_resource();
};

}  // namespace roscraft::bridge
