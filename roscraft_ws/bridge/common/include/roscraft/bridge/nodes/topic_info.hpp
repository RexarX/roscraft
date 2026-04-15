#pragma once

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <functional>

namespace roscraft::bridge {

/// @brief Serves `TopicInfoCmd` requests.
class TopicInfoNode final : public rclcpp::Node {
public:
  /// @brief Construct topic-info service node.
  /// @param incoming Incoming command queue
  /// @param outgoing Outgoing command queue
  TopicInfoNode(CommandQueue& incoming, CommandQueue& outgoing);
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
};

}  // namespace roscraft::bridge
