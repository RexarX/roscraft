#include <pch.hpp>

#include <roscraft/bridge/nodes/topic_relay.hpp>

#include <rclcpp/logging.hpp>
#include <rclcpp/qos.hpp>
#include <rclcpp/serialized_message.hpp>

#include <memory>
#include <memory_resource>
#include <string>

namespace roscraft::bridge {

void TopicRelayNode::DrainSubscribeCommands() {
  auto& storage = incoming_.get().TypedStorage<SubscribeTopicCmd>();

  std::string topic;
  std::string type;
  topic.reserve(128);
  type.reserve(128);

  SubscribeTopicCmd cmd(std::pmr::get_default_resource());
  while (storage.Dequeue(subscribe_consumer_, cmd)) {
    topic.assign(cmd.topic_name);
    type.assign(cmd.message_type);

    if (subscriptions_.contains(topic)) {
      RCLCPP_DEBUG(this->get_logger(),
                   "Already subscribed to '%s', ignoring duplicate request.",
                   topic.c_str());
      continue;
    }

    Subscribe(topic, type);
  }
}

void TopicRelayNode::Subscribe(const std::string& topic_name,
                               const std::string& message_type) {
  // GenericSubscription lets us receive raw CDR bytes without knowing the
  // concrete message type at compile time.
  auto sub = this->create_generic_subscription(
      topic_name, message_type, rclcpp::QoS(10),
      [this, topic_name,
       message_type](std::shared_ptr<rclcpp::SerializedMessage> msg) {
        // Build a TopicPayloadCmd from the raw CDR buffer.
        // Use the default PMR resource — the command lives until the network
        // layer has serialised and sent it.
        TopicPayloadCmd cmd(std::pmr::get_default_resource());
        cmd.topic_name =
            std::pmr::string(topic_name, std::pmr::get_default_resource());
        cmd.message_type =
            std::pmr::string(message_type, std::pmr::get_default_resource());

        const auto& buf = msg->get_rcl_serialized_message();
        cmd.payload.assign(buf.buffer, buf.buffer + buf.buffer_length);

        outgoing_.get().Enqueue(payload_producer_, std::move(cmd));
      });

  if (sub == nullptr) {
    RCLCPP_ERROR(this->get_logger(),
                 "Failed to create subscription for topic '%s' (type '%s').",
                 topic_name.c_str(), message_type.c_str());
    return;
  }

  RCLCPP_INFO(this->get_logger(), "Subscribed to '%s' (%s).",
              topic_name.c_str(), message_type.c_str());
  subscriptions_.emplace(topic_name, std::move(sub));
}

}  // namespace roscraft::bridge
