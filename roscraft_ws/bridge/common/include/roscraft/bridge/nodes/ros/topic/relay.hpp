#pragma once

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/nodes/ros/details/introspection_codec.hpp>
#include <roscraft/memory/arena_allocator.hpp>
#include <roscraft/utils/string_hash.hpp>

#include <rclcpp/generic_publisher.hpp>
#include <rclcpp/generic_subscription.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <cstdint>
#include <functional>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace roscraft::bridge {

class TopicPublishMessageCmd;
class TopicSubscribeCmd;
class TopicUnsubscribeCmd;

/// @brief Services `TopicSubscribeCmd` requests and forwards topic messages
/// as `TopicPayloadCmd` entries in the outgoing queue.
/// @details Each `TopicSubscribeCmd` specifies a topic name and a message type
/// string (e.g. `"geometry_msgs/msg/Twist"`). The node creates a
/// `rclcpp::GenericSubscription` for that topic so it can forward serialized
/// payload bytes without knowing the concrete message type at compile time.
///
/// Multiple subscribe requests for the same topic are tracked per request id.
/// The underlying ROS subscription is shared, while per-request options
/// (`--once`, `--timeout`, `--raw`) are applied independently.
///
/// If subscription or publisher creation fails (e.g. invalid type), an
/// `ErrorCmd` is enqueued and the bridge keeps running.
///
/// Thread-safety: all subscription callbacks and the drain-timer callback run
/// on the single ROS spin thread, so internal maps are never accessed
/// concurrently.
class TopicRelayNode final : public rclcpp::Node {
public:
  /// @brief Construct a `TopicRelayNode`.
  /// @param incoming Incoming command queue (reads `TopicSubscribeCmd`)
  /// @param outgoing Outgoing command queue (writes `TopicPayloadCmd` and
  /// `ErrorCmd`)
  /// @param allocator The memory resource for command allocation (default:
  /// `std::pmr::get_default_resource()`)
  TopicRelayNode(
      CommandQueue& incoming, CommandQueue& outgoing,
      std::pmr::memory_resource* allocator = std::pmr::get_default_resource());

  TopicRelayNode(CommandQueue& incoming, CommandQueue& outgoing,
                 std::nullptr_t) = delete;

  TopicRelayNode(const TopicRelayNode&) = delete;
  TopicRelayNode(TopicRelayNode&&) = delete;
  ~TopicRelayNode() override = default;

  TopicRelayNode& operator=(const TopicRelayNode&) = delete;
  TopicRelayNode& operator=(TopicRelayNode&&) = delete;

private:
  struct EchoRequest {
    uint64_t request_id = 0;
    bool once = false;
    bool raw = false;
  };

  struct TopicSubscription {
    std::string topic_name;
    std::string message_type;
    rclcpp::GenericSubscription::SharedPtr subscription;
    std::vector<EchoRequest> requests;
  };

  /// @brief Drain all pending `TopicSubscribeCmd`s and create subscriptions.
  void DrainSubscribeCommands();

  /// @brief Drain all pending `TopicUnsubscribeCmd`s and remove subscriptions.
  void DrainUnsubscribeCommands();

  /// @brief Drain all pending `TopicPublishMessageCmd`s and publish them.
  void DrainPublishCommands();

  /// @brief Register subscription request and ensure backing subscription.
  void Subscribe(const TopicSubscribeCmd& cmd);

  /// @brief Remove all active echo subscriptions for a topic.
  void Unsubscribe(const TopicUnsubscribeCmd& cmd);

  /// @brief Create backing generic subscription for one topic.
  /// @param topic_name Topic name
  /// @param request_id Request id for error reporting
  /// @param message_type Message type string
  /// @return True on success
  [[nodiscard]] bool CreateTopicSubscription(std::string_view topic_name,
                                             uint64_t request_id,
                                             std::string_view message_type);

  /// @brief Handle one incoming serialized message for a topic.
  /// @param topic_name Topic name
  /// @param message_type Message type
  /// @param msg Serialized message payload
  void HandleTopicMessage(std::string_view topic_name,
                          std::string_view message_type,
                          const rclcpp::SerializedMessage& msg);

  /// @brief Remove one request registration for a topic subscription.
  /// @param topic_name Topic name
  /// @param request_id Request id
  void RemoveTopicRequest(std::string_view topic_name, uint64_t request_id);

  /// @brief Remove timeout timer for request id.
  /// @param request_id Request id
  void ClearTimeout(uint64_t request_id);

  /// @brief Create or reuse publisher for `topic_name` / `message_type`.
  /// @param topic_name Topic name
  /// @param request_id Request id for error reporting
  /// @param message_type Message type
  /// @return Publisher handle when available
  [[nodiscard]] auto EnsurePublisher(std::string_view topic_name,
                                     uint64_t request_id,
                                     std::string_view message_type,
                                     std::string_view qos_profile)
      -> std::optional<std::reference_wrapper<rclcpp::GenericPublisher>>;

  /// @brief Load and cache message introspection metadata.
  /// @param message_type Message type
  /// @return Cached introspection metadata when available
  [[nodiscard]] auto EnsureMessageIntrospection(std::string_view message_type)
      -> const details::MessageIntrospection*;

  /// @brief Convert UTF-8 YAML payload into serialized CDR bytes.
  /// @param request_id Request id for error reporting
  /// @param topic_name Topic name for error reporting
  /// @param message_type Message type
  /// @param payload UTF-8 YAML bytes
  /// @return CDR payload bytes on success
  [[nodiscard]] auto SerializeYamlPayload(uint64_t request_id,
                                          std::string_view topic_name,
                                          std::string_view message_type,
                                          std::span<const uint8_t> payload)
      -> std::optional<std::vector<uint8_t>>;

  /// @brief Publish one serialized payload to a ROS topic.
  /// @param request_id Request id for error reporting
  /// @param topic_name Topic name
  /// @param message_type Topic message type
  /// @param payload Serialized payload bytes
  /// @param qos_profile QoS profile name
  /// @return True when publish succeeds
  [[nodiscard]] bool PublishSerializedMessage(uint64_t request_id,
                                              std::string_view topic_name,
                                              std::string_view message_type,
                                              std::span<const uint8_t> payload,
                                              std::string_view qos_profile);

  /// @brief Cancel and remove a repeated publish timer.
  /// @param request_id Request id
  void ClearPublishTimer(uint64_t request_id);

  /// @brief Publish one message command.
  /// @param cmd Publish command
  void Publish(const TopicPublishMessageCmd& cmd);

  /// @brief Enqueue an `ErrorCmd` to the outgoing queue.
  /// @param request_id Request id for error reporting
  /// @param error_code Error code
  /// @param error_message Error message
  void SendError(uint64_t request_id, std::string_view error_code,
                 std::string_view error_message);

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken subscribe_consumer_;
  CommandQueueConsumerToken unsubscribe_consumer_;
  CommandQueueConsumerToken publish_consumer_;
  CommandQueueProducerToken payload_producer_;
  CommandQueueProducerToken error_producer_;

  /// Periodic timer that drains new subscription requests.
  rclcpp::TimerBase::SharedPtr drain_timer_;

  /// topic_name -> active subscription state
  std::unordered_map<std::string, TopicSubscription, utils::StringHash,
                     utils::StringEqual>
      subscriptions_;

  /// topic_name -> active publisher handle
  std::unordered_map<std::string, rclcpp::GenericPublisher::SharedPtr,
                     utils::StringHash, utils::StringEqual>
      publishers_;

  /// topic_name -> publisher type
  std::unordered_map<std::string, std::string, utils::StringHash,
                     utils::StringEqual>
      publisher_types_;

  /// topic_name -> publisher qos profile
  std::unordered_map<std::string, std::string, utils::StringHash,
                     utils::StringEqual>
      publisher_qos_profiles_;

  /// message_type -> cached message introspection metadata
  std::unordered_map<std::string, std::optional<details::MessageIntrospection>,
                     utils::StringHash, utils::StringEqual>
      message_introspection_cache_;

  /// request_id -> topic name for active request
  std::unordered_map<uint64_t, std::string> once_topics_;

  /// request_id -> timeout timer
  std::unordered_map<uint64_t, rclcpp::TimerBase::SharedPtr> timeout_timers_;

  /// request_id -> repeated publish timer
  std::unordered_map<uint64_t, rclcpp::TimerBase::SharedPtr> publish_timers_;

  std::pmr::memory_resource* allocator_ = std::pmr::get_default_resource();
  memory::ArenaAllocator scratch_arena_{128 * 1024};
};

}  // namespace roscraft::bridge
