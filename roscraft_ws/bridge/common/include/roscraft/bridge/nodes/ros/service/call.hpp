#pragma once

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/service.hpp>
#include <roscraft/bridge/nodes/ros/details/introspection_codec.hpp>
#include <roscraft/memory/arena_allocator.hpp>
#include <roscraft/utils/string_hash.hpp>

#include <rclcpp/callback_group.hpp>
#include <rclcpp/generic_client.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace roscraft::bridge {

/// @brief Serves `ServiceCallCmd` requests with runtime introspection.
class ServiceCallNode final : public rclcpp::Node {
public:
  /// @brief Construct service-call node.
  /// @param incoming Incoming command queue
  /// @param outgoing Outgoing command queue
  /// @param allocator The memory resource for command allocation (default:
  /// `std::pmr::get_default_resource()`)
  ServiceCallNode(
      CommandQueue& incoming, CommandQueue& outgoing,
      std::pmr::memory_resource* allocator = std::pmr::get_default_resource());

  ServiceCallNode(CommandQueue& incoming, CommandQueue& outgoing,
                  std::nullptr_t) = delete;

  ServiceCallNode(const ServiceCallNode&) = delete;
  ServiceCallNode(ServiceCallNode&&) = delete;
  ~ServiceCallNode() override;

  ServiceCallNode& operator=(const ServiceCallNode&) = delete;
  ServiceCallNode& operator=(ServiceCallNode&&) = delete;

private:
  struct GenericServiceClientEntry {
    std::string service_type;
    rclcpp::GenericClient::SharedPtr client;
    details::ServiceIntrospection introspection;
  };

  /// @brief Drain pending `ServiceCallCmd` commands.
  void DrainServiceCallCommands();

  /// @brief Dispatch one service-call command, including repeat scheduling.
  void DispatchServiceCall(const ServiceCallCmd& cmd);

  /// @brief Execute a single service call.
  /// @return True if command is valid for repeat scheduling, false otherwise
  [[nodiscard]] bool InvokeServiceCall(uint64_t request_id,
                                       std::string_view service_name,
                                       std::string_view service_type,
                                       std::span<const uint8_t> payload,
                                       double timeout_seconds);

  /// @brief Execute one generic service call through `rclcpp::GenericClient`.
  /// @return The response text on success, or an error string on failure.
  [[nodiscard]] auto CallGenericService(std::string_view service_name,
                                        std::string_view service_type,
                                        std::span<const uint8_t> payload,
                                        std::chrono::nanoseconds timeout,
                                        std::vector<uint8_t>& response_payload)
      -> std::expected<std::string, std::string>;

  /// @brief Get or create generic client entry for a service endpoint.
  [[nodiscard]] auto EnsureGenericClient(std::string_view service_name,
                                         std::string_view service_type)
      -> std::expected<std::reference_wrapper<GenericServiceClientEntry>,
                       std::string>;

  /// @brief Serialize UTF-8 YAML request payload into CDR bytes.
  [[nodiscard]] auto SerializeRequestPayload(
      std::span<const uint8_t> payload,
      const details::MessageIntrospection& introspection)
      -> details::IntrospectionCodecResult<std::vector<uint8_t>>;

  /// @brief Deserialize CDR response bytes into UTF-8 text.
  [[nodiscard]] auto DeserializeResponsePayload(
      std::span<const uint8_t> payload,
      const details::MessageIntrospection& introspection)
      -> details::IntrospectionCodecResult<std::string>;

  /// @brief Periodic callback.
  void OnPollTimer();

  /// @brief Cancel and remove repeated-call timer for request id.
  void ClearRepeatTimer(uint64_t request_id);

  /// @brief Enqueue a service-call response command.
  void SendResponse(uint64_t request_id, std::string_view service_name,
                    std::string_view service_type, bool success,
                    std::span<const uint8_t> response_payload,
                    std::string_view result_text);

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken service_call_consumer_;
  CommandQueueProducerToken service_call_response_producer_;

  rclcpp::TimerBase::SharedPtr poll_timer_;
  rclcpp::CallbackGroup::SharedPtr client_group_;

  std::unordered_map<std::string, GenericServiceClientEntry, utils::StringHash,
                     utils::StringEqual>
      generic_service_clients_;

  std::unordered_map<uint64_t, rclcpp::TimerBase::SharedPtr> repeat_timers_;

  std::pmr::memory_resource* allocator_ = std::pmr::get_default_resource();
  memory::ArenaAllocator scratch_arena_{128 * 1024};
};

}  // namespace roscraft::bridge
