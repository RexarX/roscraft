#pragma once

#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <functional>
#include <memory_resource>

namespace roscraft::bridge {

/// @brief Serves `TopicInfoCmd` requests.
class TopicInfoNode final : public rclcpp::Node {
public:
  /// @brief Construct topic-info service node.
  /// @param incoming Incoming command queue
  /// @param outgoing Outgoing command queue
  /// @param allocator The memory resource for command allocation (default:
  /// `std::pmr::get_default_resource()`)
  TopicInfoNode(
      CommandQueue& incoming, CommandQueue& outgoing,
      std::pmr::memory_resource* allocator = std::pmr::get_default_resource());

  TopicInfoNode(CommandQueue& incoming, CommandQueue& outgoing,
                std::nullptr_t) = delete;

  TopicInfoNode(const TopicInfoNode&) = delete;
  TopicInfoNode(TopicInfoNode&&) = delete;
  ~TopicInfoNode() override = default;

  TopicInfoNode& operator=(const TopicInfoNode&) = delete;
  TopicInfoNode& operator=(TopicInfoNode&&) = delete;

private:
  /// @brief Drain pending `TopicInfoCmd` commands.
  void DrainTopicInfoCommands();

  /// @brief Periodic callback.
  void OnPollTimer();

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken topic_info_consumer_;
  CommandQueueProducerToken topic_info_response_producer_;

  rclcpp::TimerBase::SharedPtr poll_timer_;

  std::pmr::memory_resource* allocator_ = std::pmr::get_default_resource();
};

}  // namespace roscraft::bridge
