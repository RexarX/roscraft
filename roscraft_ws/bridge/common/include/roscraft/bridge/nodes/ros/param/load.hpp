#pragma once

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/utils/string_hash.hpp>

#include <rclcpp/node.hpp>
#include <rclcpp/parameter_client.hpp>
#include <rclcpp/timer.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <unordered_map>

namespace roscraft::bridge {

/// @brief Serves `param load` requests.
class ParamLoadNode final : public rclcpp::Node {
public:
  /// @brief Construct param load node.
  /// @param incoming Incoming command queue
  /// @param outgoing Outgoing command queue
  /// @param allocator The memory resource for command allocation (default:
  /// `std::pmr::get_default_resource()`)
  ParamLoadNode(
      CommandQueue& incoming, CommandQueue& outgoing,
      std::pmr::memory_resource* allocator = std::pmr::get_default_resource());

  ParamLoadNode(CommandQueue& incoming, CommandQueue& outgoing,
                std::nullptr_t) = delete;

  ParamLoadNode(const ParamLoadNode&) = delete;
  ParamLoadNode(ParamLoadNode&&) = delete;
  ~ParamLoadNode() override = default;

  ParamLoadNode& operator=(const ParamLoadNode&) = delete;
  ParamLoadNode& operator=(ParamLoadNode&&) = delete;

private:
  /// @brief Drain pending `ParamLoadCmd` commands.
  void DrainParamLoadCommands();

  /// @brief Periodic callback.
  void OnPollTimer();

  /// @brief Enqueue an `ErrorCmd` to the outgoing queue.
  void SendError(uint64_t request_id, std::string_view error_code,
                 std::string_view error_message);

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken param_load_consumer_;
  CommandQueueProducerToken param_load_response_producer_;
  CommandQueueProducerToken error_producer_;

  rclcpp::TimerBase::SharedPtr poll_timer_;

  std::unordered_map<std::string, std::shared_ptr<rclcpp::SyncParametersClient>,
                     utils::StringHash, utils::StringEqual>
      parameter_clients_;

  std::pmr::memory_resource* allocator_ = std::pmr::get_default_resource();
};

}  // namespace roscraft::bridge
