#pragma once

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <functional>

namespace roscraft::bridge {

/// @brief Serves `ServiceInfoCmd` requests.
class ServiceInfoNode final : public rclcpp::Node {
public:
  /// @brief Construct service-info service node.
  /// @param incoming Incoming command queue
  /// @param outgoing Outgoing command queue
  ServiceInfoNode(CommandQueue& incoming, CommandQueue& outgoing);
  ServiceInfoNode(const ServiceInfoNode&) = delete;
  ServiceInfoNode(ServiceInfoNode&&) = delete;
  ~ServiceInfoNode() override = default;

  ServiceInfoNode& operator=(const ServiceInfoNode&) = delete;
  ServiceInfoNode& operator=(ServiceInfoNode&&) = delete;

private:
  /// @brief Drain pending `ServiceInfoCmd` commands.
  void DrainServiceInfoCommands();

  /// @brief Periodic callback.
  void OnPollTimer();

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken service_info_consumer_;
  CommandQueueProducerToken service_info_response_producer_;

  rclcpp::TimerBase::SharedPtr poll_timer_;
};

}  // namespace roscraft::bridge
