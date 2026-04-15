#include <pch.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/nodes/topic_relay.hpp>

#include <rclcpp/generic_publisher.hpp>
#include <rclcpp/qos.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/serialized_message.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>

namespace roscraft::bridge {

TopicRelayNode::TopicRelayNode(CommandQueue& incoming, CommandQueue& outgoing)
    : rclcpp::Node("roscraft_topic_relay_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      subscribe_consumer_(incoming.MakeConsumerToken<SubscribeTopicCmd>()),
      publish_consumer_(incoming.MakeConsumerToken<PublishMessageCmd>()),
      payload_producer_(outgoing.MakeProducerToken<TopicPayloadCmd>()),
      error_producer_(outgoing.MakeProducerToken<ErrorCmd>()) {
  using namespace std::chrono_literals;
  // Poll for new subscription requests at 100 ms — fast enough to feel
  // responsive, cheap enough to not waste spin budget.
  drain_timer_ = this->create_wall_timer(100ms, [this] {
    DrainSubscribeCommands();
    DrainPublishCommands();
  });
}

void TopicRelayNode::DrainSubscribeCommands() {
  auto& storage = incoming_.get().TypedStorage<SubscribeTopicCmd>();

  SubscribeTopicCmd cmd(std::pmr::get_default_resource());
  while (storage.Dequeue(subscribe_consumer_, cmd)) {
    Subscribe(cmd);
  }
}

void TopicRelayNode::DrainPublishCommands() {
  auto& storage = incoming_.get().TypedStorage<PublishMessageCmd>();

  PublishMessageCmd cmd(std::pmr::get_default_resource());
  while (storage.Dequeue(publish_consumer_, cmd)) {
    Publish(cmd);
  }
}

void TopicRelayNode::Subscribe(const SubscribeTopicCmd& cmd) {
  if (cmd.topic_name.empty() || cmd.message_type.empty()) [[unlikely]] {
    SendError(cmd.request_id, "SUBSCRIBE_FAILED",
              "Topic name and message type must be non-empty");
    return;
  }

  const auto it = subscriptions_.find(cmd.topic_name);
  if (it == subscriptions_.end()) {
    if (!CreateTopicSubscription(cmd.topic_name, cmd.request_id,
                                 cmd.message_type)) {
      return;
    }
  }

  auto& state = it->second;
  if (state.message_type != std::string_view(cmd.message_type)) [[unlikely]] {
    SendError(
        cmd.request_id, "SUBSCRIBE_FAILED",
        std::format(
            "Topic '{}' already subscribed with type '{}', requested '{}'",
            cmd.topic_name, state.message_type, cmd.message_type));
    return;
  }

  const bool already_registered =
      std::ranges::any_of(state.requests, [&cmd](const EchoRequest& request) {
        return request.request_id == cmd.request_id;
      });
  if (already_registered) {
    return;
  }

  state.requests.push_back(EchoRequest{
      .request_id = cmd.request_id, .once = cmd.once, .raw = cmd.raw});
  once_topics_[cmd.request_id].assign(cmd.topic_name);

  if (cmd.timeout_seconds > 0.0) {
    const auto timeout_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double>(cmd.timeout_seconds));
    auto timer = this->create_wall_timer(
        timeout_ns, [this, request_id = cmd.request_id] {
          if (!once_topics_.contains(request_id)) {
            ClearTimeout(request_id);
            return;
          }

          const std::string topic = once_topics_.at(request_id);
          RemoveTopicRequest(topic, request_id);
          SendError(request_id, "SUBSCRIBE_TIMEOUT",
                    std::format("No message received before timeout for '{}'",
                                topic));
        });
    timeout_timers_[cmd.request_id] = std::move(timer);
  }

  RCLCPP_INFO(
      this->get_logger(),
      "Registered echo request %lu for '%s' (%s) once=%s raw=%s timeout=%.3f",
      static_cast<unsigned long>(cmd.request_id), cmd.topic_name.c_str(),
      cmd.message_type.c_str(), cmd.once ? "true" : "false",
      cmd.raw ? "true" : "false", cmd.timeout_seconds);
}

bool TopicRelayNode::CreateTopicSubscription(std::string_view topic_name,
                                             uint64_t request_id,
                                             std::string_view message_type) {
  try {
    std::string topic_name_str(topic_name);
    std::string message_type_str(message_type);
    auto subscription = this->create_generic_subscription(
        topic_name_str, message_type_str, rclcpp::QoS(10),
        [this, topic = topic_name_str, type = message_type_str](
            std::shared_ptr<rclcpp::SerializedMessage> msg) {
          HandleTopicMessage(topic, type, *msg);
        });

    if (subscription == nullptr) [[unlikely]] {
      SendError(
          request_id, "SUBSCRIBE_FAILED",
          std::format("Failed to create subscription for '{}'", topic_name));
      return false;
    }

    TopicSubscription state{
        .topic_name = topic_name_str,
        .message_type = std::move(message_type_str),
        .subscription = std::move(subscription),
        .requests = {},
    };

    subscriptions_.emplace(std::move(topic_name_str), std::move(state));
    return true;
  } catch (const std::exception& ex) {
    SendError(request_id, "SUBSCRIBE_FAILED", ex.what());
    return false;
  } catch (...) {
    SendError(request_id, "SUBSCRIBE_FAILED",
              std::format("Unknown subscribe error for '{}'", topic_name));
    return false;
  }
}

void TopicRelayNode::HandleTopicMessage(std::string_view topic_name,
                                        std::string_view message_type,
                                        const rclcpp::SerializedMessage& msg) {
  const auto it = subscriptions_.find(topic_name);
  if (it == subscriptions_.end()) [[unlikely]] {
    return;
  }

  auto& state = it->second;
  if (state.requests.empty()) [[unlikely]] {
    return;
  }

  const auto& serialized = msg.get_rcl_serialized_message();
  std::vector<uint64_t> one_shot_requests;
  one_shot_requests.reserve(state.requests.size());

  for (const auto& request : state.requests) {
    TopicPayloadCmd out_cmd(std::pmr::get_default_resource());
    out_cmd.request_id = request.request_id;
    out_cmd.topic_name =
        std::pmr::string(topic_name, std::pmr::get_default_resource());
    out_cmd.message_type =
        std::pmr::string(message_type, std::pmr::get_default_resource());
    out_cmd.raw = request.raw;
    out_cmd.payload.assign(serialized.buffer,
                           serialized.buffer + serialized.buffer_length);

    outgoing_.get().Enqueue(payload_producer_, std::move(out_cmd));

    if (request.once) {
      one_shot_requests.push_back(request.request_id);
    }
  }

  for (const auto request_id : one_shot_requests) {
    RemoveTopicRequest(topic_name, request_id);
  }
}

void TopicRelayNode::RemoveTopicRequest(std::string_view topic_name,
                                        uint64_t request_id) {
  const auto it = subscriptions_.find(topic_name);
  if (it == subscriptions_.end()) [[unlikely]] {
    once_topics_.erase(request_id);
    ClearTimeout(request_id);
    return;
  }

  auto& state = it->second;
  state.requests.erase(
      std::remove_if(state.requests.begin(), state.requests.end(),
                     [request_id](const EchoRequest& request) {
                       return request.request_id == request_id;
                     }),
      state.requests.end());

  once_topics_.erase(request_id);
  ClearTimeout(request_id);

  if (auto it = subscriptions_.find(topic_name); it != subscriptions_.end()) {
    subscriptions_.erase(it);
  }
}

void TopicRelayNode::ClearTimeout(uint64_t request_id) {
  if (!timeout_timers_.contains(request_id)) [[unlikely]] {
    return;
  }

  timeout_timers_.at(request_id)->cancel();
  timeout_timers_.erase(request_id);
}

auto TopicRelayNode::EnsurePublisher(std::string_view topic_name,
                                     uint64_t request_id,
                                     std::string_view message_type)
    -> std::optional<std::reference_wrapper<rclcpp::GenericPublisher>> {
  const auto topic_it = publishers_.find(topic_name);
  if (topic_it != publishers_.end()) {
    const auto type_it = publisher_types_.find(topic_name);
    if (type_it != publisher_types_.end() && type_it->second != message_type) {
      SendError(
          request_id, "PUBLISH_FAILED",
          std::format(
              "Topic '{}' already published with type '{}', requested '{}'",
              topic_name, type_it->second, message_type));
      return std::nullopt;
    }
    auto& publisher = topic_it->second;
    return std::ref(*publisher);
  }

  std::string topic_str(topic_name);
  std::string type_str(message_type);

  try {
    auto publisher =
        this->create_generic_publisher(topic_str, type_str, rclcpp::QoS(10));
    if (publisher == nullptr) [[unlikely]] {
      SendError(request_id, "PUBLISH_FAILED",
                std::format("Failed to create publisher for '{}'", topic_name));
      return std::nullopt;
    }

    auto [it, inserted] =
        publishers_.emplace(std::move(topic_str), std::move(publisher));
    ROSCRAFT_ASSERT(inserted, "Publisher insertion should succeed for '{}'!",
                    topic_name);
    publisher_types_.insert_or_assign(std::string(topic_name),
                                      std::move(type_str));
    return std::ref(*it->second);
  } catch (const std::exception& ex) {
    SendError(request_id, "PUBLISH_FAILED", ex.what());
    return std::nullopt;
  } catch (...) {
    SendError(request_id, "PUBLISH_FAILED",
              std::format("Unknown publish error for '{}'", topic_name));
    return std::nullopt;
  }
}

void TopicRelayNode::Publish(const PublishMessageCmd& cmd) {
  if (cmd.topic_name.empty() || cmd.message_type.empty()) [[unlikely]] {
    SendError(cmd.request_id, "PUBLISH_FAILED",
              "Topic name and message type must be non-empty");
    return;
  }

  const auto publisher =
      EnsurePublisher(cmd.topic_name, cmd.request_id, cmd.message_type);
  if (!publisher.has_value()) [[unlikely]] {
    return;
  }

  rclcpp::SerializedMessage serialized_message(cmd.payload.size());
  auto& rcl_message = serialized_message.get_rcl_serialized_message();
  ROSCRAFT_ASSERT(
      serialized_message.capacity() >= cmd.payload.size(),
      "Serialized message capacity '{}' is below payload size '{}'!",
      serialized_message.capacity(), cmd.payload.size());
  if (!cmd.payload.empty()) {
    std::ranges::copy(cmd.payload, rcl_message.buffer);
  }
  rcl_message.buffer_length = cmd.payload.size();

  try {
    publisher->get().publish(serialized_message);
  } catch (const std::exception& ex) {
    SendError(cmd.request_id, "PUBLISH_FAILED", ex.what());
  } catch (...) {
    SendError(cmd.request_id, "PUBLISH_FAILED", "Unknown publish failure");
  }
}

void TopicRelayNode::SendError(uint64_t request_id, std::string_view error_code,
                               std::string_view error_message) {
  ErrorCmd cmd(std::pmr::get_default_resource());
  cmd.request_id = request_id;
  cmd.error_code =
      std::pmr::string(error_code, std::pmr::get_default_resource());
  cmd.error_message =
      std::pmr::string(error_message, std::pmr::get_default_resource());
  outgoing_.get().Enqueue(error_producer_, std::move(cmd));
}

}  // namespace roscraft::bridge
