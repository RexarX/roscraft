#pragma once

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/generic_subscription.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace roscraft::bridge {

/// @brief Services `SubscribeTopicCmd` requests and forwards topic messages
/// as `TopicPayloadCmd` entries in the outgoing queue.
/// @details Each `SubscribeTopicCmd` specifies a topic name and a message type
/// string (e.g. `"geometry_msgs/msg/Twist"`).  The node creates a
/// `rclcpp::GenericSubscription` for that topic so it can forward raw CDR
/// bytes without knowing the concrete message type at compile time.
///
/// Duplicate subscription requests for the same topic are silently ignored.
/// Subscriptions are never torn down during the node's lifetime — if the mod
/// wants to unsubscribe it can simply stop forwarding the corresponding
/// packets (a future `UnsubscribeTopicCmd` can be added).
///
/// Thread-safety: all subscription callbacks and the drain-timer callback run
/// on the single ROS spin thread, so `subscriptions_` is never accessed
/// concurrently.
class TopicRelayNode final : public rclcpp::Node {
public:
  /// @brief Construct a `TopicRelayNode`.
  /// @param incoming Incoming command queue (reads `SubscribeTopicCmd`)
  /// @param outgoing Outgoing command queue (writes `TopicPayloadCmd`)
  TopicRelayNode(CommandQueue& incoming, CommandQueue& outgoing);
  ~TopicRelayNode() override = default;

private:
  /// @brief Drain all pending `SubscribeTopicCmd`s and create subscriptions.
  void DrainSubscribeCommands();

  /// @brief Create a `GenericSubscription` for `topic_name` / `message_type`.
  void Subscribe(const std::string& topic_name,
                 const std::string& message_type);

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken subscribe_consumer_;
  CommandQueueProducerToken payload_producer_;

  /// Periodic timer that drains new subscription requests.
  rclcpp::TimerBase::SharedPtr drain_timer_;

  /// topic_name -> active subscription handle
  std::unordered_map<std::string, rclcpp::GenericSubscription::SharedPtr>
      subscriptions_;
};

inline TopicRelayNode::TopicRelayNode(CommandQueue& incoming,
                                      CommandQueue& outgoing)
    : rclcpp::Node("roscraft_topic_relay_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      subscribe_consumer_(incoming.MakeConsumerToken<SubscribeTopicCmd>()),
      payload_producer_(outgoing.MakeProducerToken<TopicPayloadCmd>()) {
  using namespace std::chrono_literals;
  // Poll for new subscription requests at 100 ms — fast enough to feel
  // responsive, cheap enough to not waste spin budget.
  drain_timer_ = create_wall_timer(100ms, [this] { DrainSubscribeCommands(); });
}

}  // namespace roscraft::bridge
