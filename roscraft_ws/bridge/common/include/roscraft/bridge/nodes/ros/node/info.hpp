#pragma once

#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <functional>
#include <memory_resource>

namespace roscraft::bridge {

/// @brief Serves `NodeInfoCmd` requests.
class NodeInfoNode final : public rclcpp::Node {
public:
  /// @brief Construct node-info service node.
  /// @param incoming Incoming command queue
  /// @param outgoing Outgoing command queue
  /// @param allocator The memory resource for command allocation (default:
  /// `std::pmr::get_default_resource()`)
  NodeInfoNode(
      CommandQueue& incoming, CommandQueue& outgoing,
      std::pmr::memory_resource* allocator = std::pmr::get_default_resource());

  NodeInfoNode(CommandQueue& incoming, CommandQueue& outgoing,
               std::nullptr_t) = delete;

  NodeInfoNode(const NodeInfoNode&) = delete;
  NodeInfoNode(NodeInfoNode&&) = delete;
  ~NodeInfoNode() override = default;

  NodeInfoNode& operator=(const NodeInfoNode&) = delete;
  NodeInfoNode& operator=(NodeInfoNode&&) = delete;

private:
  /// @brief Drain pending `NodeInfoCmd` commands.
  void DrainNodeInfoCommands();

  /// @brief Periodic callback.
  void OnPollTimer();

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken node_info_consumer_;
  CommandQueueProducerToken node_info_response_producer_;

  rclcpp::TimerBase::SharedPtr poll_timer_;

  std::pmr::memory_resource* allocator_ = std::pmr::get_default_resource();
};

}  // namespace roscraft::bridge
