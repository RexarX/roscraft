#pragma once

#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <functional>
#include <memory_resource>

namespace roscraft::bridge {

/// @brief Serves `ServiceInfoCmd` requests.
class ServiceInfoNode final : public rclcpp::Node {
public:
  /// @brief Construct service-info service node.
  /// @param incoming Incoming command queue
  /// @param outgoing Outgoing command queue
  /// @param allocator The memory resource for command allocation (default:
  /// `std::pmr::get_default_resource()`)
  ServiceInfoNode(
      CommandQueue& incoming, CommandQueue& outgoing,
      std::pmr::memory_resource* allocator = std::pmr::get_default_resource());

  ServiceInfoNode(CommandQueue& incoming, CommandQueue& outgoing,
                  std::nullptr_t) = delete;

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

  std::pmr::memory_resource* allocator_ = std::pmr::get_default_resource();
};

}  // namespace roscraft::bridge
