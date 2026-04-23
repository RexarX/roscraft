#pragma once

#include <roscraft/bridge/command/queue.hpp>

#include <roscraft/bridge/nodes/ros/details/introspection_codec.hpp>
#include <roscraft/memory/arena_allocator.hpp>
#include <roscraft/utils/string_hash.hpp>

#include <rclcpp/callback_group.hpp>
#include <rclcpp/generic_client.hpp>
#include <rclcpp/generic_subscription.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace roscraft::bridge {

struct ActionSendGoalCmd;

/// @brief Serves `ActionSendGoalCmd` requests with runtime introspection.
/// @details Uses `rclcpp::GenericClient` and `rclcpp::GenericSubscription` to
/// communicate with any action type without compile-time type dependencies.
class ActionSendGoalNode final : public rclcpp::Node {
public:
  /// @brief Constructs an `ActionSendGoalNode` with the given command queues
  /// and memory resource.
  /// @param incoming The incoming command queue
  /// @param outgoing The outgoing command queue
  /// @param allocator The memory resource for command allocation (default:
  /// `std::pmr::get_default_resource()`)
  ActionSendGoalNode(
      CommandQueue& incoming, CommandQueue& outgoing,
      std::pmr::memory_resource* allocator = std::pmr::get_default_resource());

  ActionSendGoalNode(CommandQueue& incoming, CommandQueue& outgoing,
                     std::nullptr_t) = delete;

  ActionSendGoalNode(const ActionSendGoalNode&) = delete;
  ActionSendGoalNode(ActionSendGoalNode&&) = delete;
  ~ActionSendGoalNode() override = default;

  ActionSendGoalNode& operator=(const ActionSendGoalNode&) = delete;
  ActionSendGoalNode& operator=(ActionSendGoalNode&&) = delete;

private:
  /// @brief Cached generic clients, subscriptions, and introspection data
  /// for one action endpoint.
  struct GenericActionClientEntry {
    std::string action_type;
    rclcpp::GenericClient::SharedPtr send_goal_client;
    rclcpp::GenericClient::SharedPtr get_result_client;
    rclcpp::GenericClient::SharedPtr cancel_goal_client;
    rclcpp::GenericSubscription::SharedPtr feedback_subscription;
    details::ServiceIntrospection send_goal_introspection;
    details::ServiceIntrospection get_result_introspection;
    details::ServiceIntrospection cancel_goal_introspection;
    details::MessageIntrospection feedback_message_introspection;
  };

  /// @brief Tracks one in-flight action goal from dispatch to completion.
  struct ActionGoalSession {
    uint64_t request_id = 0;
    std::string action_name;
    std::string action_type;
    std::array<uint8_t, 16> goal_uuid = {};
    bool feedback = false;
    bool completed = false;
    rclcpp::TimerBase::SharedPtr timeout_timer;
    std::function<void()> cancel_goal;
    std::optional<rclcpp::GenericClient::FutureAndRequestId> get_result_future;
  };

  void DrainActionSendGoalCommands();
  void DispatchSendGoal(const ActionSendGoalCmd& cmd);
  void OnGoalTimeout(uint64_t request_id);
  void OnPollTimer();

  /// @brief Get or create a `GenericActionClientEntry` for the given action
  /// endpoint.
  [[nodiscard]] auto EnsureGenericClientEntry(std::string_view action_name,
                                              std::string_view action_type)
      -> std::expected<std::reference_wrapper<GenericActionClientEntry>,
                       std::string>;

  /// @brief Dispatch a goal using generic introspection-based clients.
  bool DispatchGenericGoal(const ActionSendGoalCmd& cmd,
                           std::span<const uint8_t> payload,
                           GenericActionClientEntry& entry);

  /// @brief Handle an incoming feedback message from a generic subscription.
  void HandleFeedbackMessage(std::string_view action_name,
                             std::string_view action_type,
                             std::span<const uint8_t> serialized_data);

  /// @brief Poll all active sessions for completed get_result futures.
  void CheckGetResultFutures();

  void SendFeedback(uint64_t request_id, std::string_view action_name,
                    std::string_view action_type,
                    std::span<const uint8_t> feedback_payload,
                    std::string_view feedback_text);

  void SendResult(uint64_t request_id, std::string_view action_name,
                  std::string_view action_type, bool success,
                  std::span<const uint8_t> result_payload,
                  std::string_view result_text);

  void RemoveSession(uint64_t request_id);

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken action_send_goal_consumer_;
  CommandQueueProducerToken action_feedback_producer_;
  CommandQueueProducerToken action_result_producer_;

  rclcpp::TimerBase::SharedPtr poll_timer_;
  rclcpp::CallbackGroup::SharedPtr action_group_;

  std::unordered_map<std::string, GenericActionClientEntry, utils::StringHash,
                     utils::StringEqual>
      generic_action_clients_;

  std::unordered_map<uint64_t, ActionGoalSession> sessions_;

  std::pmr::memory_resource* allocator_ = std::pmr::get_default_resource();
  memory::ArenaAllocator scratch_arena_{128 * 1024};
};

}  // namespace roscraft::bridge
