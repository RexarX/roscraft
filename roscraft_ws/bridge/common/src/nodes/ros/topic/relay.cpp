#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/topic/relay.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/error.hpp>
#include <roscraft/bridge/command/types/topic.hpp>
#include <roscraft/bridge/nodes/ros/details/introspection_codec.hpp>
#include <roscraft/memory/arena_allocator.hpp>

#include <rclcpp/generic_publisher.hpp>
#include <rclcpp/qos.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/serialized_message.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace roscraft::bridge {

namespace {

[[nodiscard]] std::pmr::string NormalizeQosProfile(
    std::string_view qos_profile,
    std::pmr::memory_resource* allocator = std::pmr::get_default_resource()) {
  std::pmr::string normalized(qos_profile, allocator);
  if (normalized.empty()) {
    normalized = "default";
  }

  std::ranges::transform(normalized, normalized.begin(), [](unsigned char ch) {
    if (ch == '-') {
      return '_';
    }
    return static_cast<char>(std::tolower(ch));
  });
  return normalized;
}

[[nodiscard]] auto QosFromProfile(std::string_view qos_profile)
    -> std::optional<rclcpp::QoS> {
  if (qos_profile == "default") {
    return rclcpp::QoS(10);
  }
  if (qos_profile == "sensor_data") {
    return rclcpp::SensorDataQoS();
  }
  if (qos_profile == "services_default") {
    return rclcpp::ServicesQoS();
  }
  if (qos_profile == "parameters") {
    return rclcpp::ParametersQoS();
  }
  if (qos_profile == "parameter_events") {
    return rclcpp::ParameterEventsQoS();
  }
  if (qos_profile == "system_default") {
    return rclcpp::SystemDefaultsQoS();
  }
  return std::nullopt;
}

}  // namespace

TopicRelayNode::TopicRelayNode(CommandQueue& incoming, CommandQueue& outgoing,
                               std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_topic_relay_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      subscribe_consumer_(incoming.MakeConsumerToken<TopicSubscribeCmd>()),
      publish_consumer_(incoming.MakeConsumerToken<TopicPublishMessageCmd>()),
      payload_producer_(outgoing.MakeProducerToken<TopicPayloadCmd>()),
      error_producer_(outgoing.MakeProducerToken<ErrorCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  // Poll for new subscription requests at 100 ms — fast enough to feel
  // responsive, cheap enough to not waste spin budget.
  drain_timer_ = this->create_wall_timer(50ms, [this] {
    DrainSubscribeCommands();
    DrainPublishCommands();
    scratch_arena_.Reset();
  });
}

void TopicRelayNode::DrainSubscribeCommands() {
  auto& storage = incoming_.get().TypedStorage<TopicSubscribeCmd>();

  TopicSubscribeCmd cmd(allocator_);
  while (storage.Dequeue(subscribe_consumer_, cmd)) {
    Subscribe(cmd);
  }
}

void TopicRelayNode::DrainPublishCommands() {
  auto& storage = incoming_.get().TypedStorage<TopicPublishMessageCmd>();

  TopicPublishMessageCmd cmd(allocator_);
  while (storage.Dequeue(publish_consumer_, cmd)) {
    Publish(cmd);
  }
}

void TopicRelayNode::Subscribe(const TopicSubscribeCmd& cmd) {
  if (cmd.topic_name.empty()) [[unlikely]] {
    SendError(cmd.request_id, "SUBSCRIBE_FAILED",
              "Topic name must be non-empty");
    return;
  }

  std::pmr::string resolved_message_type(&scratch_arena_);
  auto it = subscriptions_.find(cmd.topic_name);
  if (cmd.message_type.empty()) {
    if (it != subscriptions_.end()) {
      resolved_message_type = it->second.message_type;
    } else {
      const auto topic_map = this->get_topic_names_and_types();
      const auto topic_it = topic_map.find(std::string(cmd.topic_name));
      if (topic_it == topic_map.end() || topic_it->second.empty())
          [[unlikely]] {
        const size_t formatted_size = std::formatted_size(
            "Unable to infer message type for topic '{}'", cmd.topic_name);
        std::pmr::string result{&scratch_arena_};
        result.resize(formatted_size);
        std::format_to(result.begin(),
                       "Unable to infer message type for topic '{}'",
                       cmd.topic_name);

        SendError(cmd.request_id, "SUBSCRIBE_FAILED", result);
        return;
      }

      resolved_message_type = topic_it->second.front();
    }
  } else {
    resolved_message_type.assign(cmd.message_type);
  }

  if (it == subscriptions_.end()) {
    if (!CreateTopicSubscription(cmd.topic_name, cmd.request_id,
                                 resolved_message_type)) {
      return;
    }

    it = subscriptions_.find(cmd.topic_name);
    ROSCRAFT_ASSERT(it != subscriptions_.end(),
                    "Topic subscription '{}' should exist after creation!",
                    cmd.topic_name);
  }

  auto& state = it->second;
  if (std::string_view(state.message_type) != resolved_message_type)
      [[unlikely]] {
    const size_t formatted_size = std::formatted_size(
        "Topic '{}' already subscribed with type '{}', requested '{}'",
        cmd.topic_name, state.message_type, resolved_message_type);
    std::pmr::string result{&scratch_arena_};
    result.resize(formatted_size);
    std::format_to(
        result.begin(),
        "Topic '{}' already subscribed with type '{}', requested '{}'",
        cmd.topic_name, state.message_type, resolved_message_type);

    SendError(cmd.request_id, "SUBSCRIBE_FAILED", result);
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
          const auto request_it = once_topics_.find(request_id);
          if (request_it == once_topics_.end()) {
            ClearTimeout(request_id);
            return;
          }

          const auto& topic = request_it->second;
          RemoveTopicRequest(topic, request_id);

          const size_t formatted_size = std::formatted_size(
              "No message received before timeout for '{}'", topic);
          std::pmr::string result{&scratch_arena_};
          result.resize(formatted_size);
          std::format_to(result.begin(),
                         "No message received before timeout for '{}'", topic);

          SendError(request_id, "SUBSCRIBE_TIMEOUT", result);
        });
    timeout_timers_[cmd.request_id] = std::move(timer);
  }

  RCLCPP_INFO(
      this->get_logger(),
      "Registered echo request %lu for '%s' (%s) once=%s raw=%s timeout=%.3f",
      static_cast<unsigned long>(cmd.request_id), cmd.topic_name.c_str(),
      resolved_message_type.c_str(), cmd.once ? "true" : "false",
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
      const size_t formatted_size = std::formatted_size(
          "Failed to create subscription for '{}'", topic_name);
      std::pmr::string result{&scratch_arena_};
      result.resize(formatted_size);
      std::format_to(result.begin(), "Failed to create subscription for '{}'",
                     topic_name);

      SendError(request_id, "SUBSCRIBE_FAILED", result);
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
    const size_t formatted_size =
        std::formatted_size("Unknown subscribe error for '{}'", topic_name);
    std::pmr::string result{&scratch_arena_};
    result.resize(formatted_size);
    std::format_to(result.begin(), "Unknown subscribe error for '{}'",
                   topic_name);

    SendError(request_id, "SUBSCRIBE_FAILED", result);
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
  std::pmr::vector<uint64_t> one_shot_requests{&scratch_arena_};
  one_shot_requests.reserve(state.requests.size());

  for (const auto& request : state.requests) {
    TopicPayloadCmd out_cmd(allocator_);
    out_cmd.request_id = request.request_id;
    out_cmd.topic_name = std::pmr::string(topic_name, allocator_);
    out_cmd.message_type = std::pmr::string(message_type, allocator_);
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

  if (state.requests.empty()) {
    subscriptions_.erase(it);
  }
}

void TopicRelayNode::ClearTimeout(uint64_t request_id) {
  const auto timer_it = timeout_timers_.find(request_id);
  if (timer_it == timeout_timers_.end()) [[unlikely]] {
    return;
  }

  timer_it->second->cancel();
  timeout_timers_.erase(timer_it);
}

auto TopicRelayNode::EnsurePublisher(std::string_view topic_name,
                                     uint64_t request_id,
                                     std::string_view message_type,
                                     std::string_view qos_profile)
    -> std::optional<std::reference_wrapper<rclcpp::GenericPublisher>> {
  const auto normalized_qos_profile =
      NormalizeQosProfile(qos_profile, &scratch_arena_);

  const auto topic_it = publishers_.find(topic_name);
  if (topic_it != publishers_.end()) {
    const auto type_it = publisher_types_.find(topic_name);
    if (type_it != publisher_types_.end() && type_it->second != message_type)
        [[unlikely]] {
      const size_t formatted_size = std::formatted_size(
          "Topic '{}' already published with type '{}', requested '{}'",
          topic_name, type_it->second, message_type);
      std::pmr::string result{&scratch_arena_};
      result.resize(formatted_size);
      std::format_to(
          result.begin(),
          "Topic '{}' already published with type '{}', requested '{}'",
          topic_name, type_it->second, message_type);

      SendError(request_id, "PUBLISH_FAILED", result);
      return std::nullopt;
    }

    const auto qos_it = publisher_qos_profiles_.find(topic_name);
    if (qos_it != publisher_qos_profiles_.end() &&
        qos_it->second != std::string_view(normalized_qos_profile))
        [[unlikely]] {
      const size_t formatted_size = std::formatted_size(
          "Topic '{}' already published with qos '{}', requested '{}'",
          topic_name, qos_it->second, normalized_qos_profile);
      std::pmr::string result{&scratch_arena_};
      result.resize(formatted_size);
      std::format_to(
          result.begin(),
          "Topic '{}' already published with qos '{}', requested '{}'",
          topic_name, qos_it->second, normalized_qos_profile);

      SendError(request_id, "PUBLISH_FAILED", result);
      return std::nullopt;
    }

    auto& publisher = topic_it->second;
    return std::ref(*publisher);
  }

  const auto qos = QosFromProfile(normalized_qos_profile);
  if (!qos.has_value()) [[unlikely]] {
    const size_t formatted_size =
        std::formatted_size("Unknown QoS profile '{}'", normalized_qos_profile);
    std::pmr::string result{&scratch_arena_};
    result.resize(formatted_size);
    std::format_to(result.begin(), "Unknown QoS profile '{}'",
                   normalized_qos_profile);

    SendError(request_id, "PUBLISH_FAILED", result);
    return std::nullopt;
  }

  std::string topic_str(topic_name);
  std::string type_str(message_type);

  try {
    auto publisher = this->create_generic_publisher(topic_str, type_str, *qos);
    if (publisher == nullptr) [[unlikely]] {
      const size_t formatted_size = std::formatted_size(
          "Failed to create publisher for '{}'", topic_name);
      std::pmr::string result{&scratch_arena_};
      result.resize(formatted_size);
      std::format_to(result.begin(), "Failed to create publisher for '{}'",
                     topic_name);

      SendError(request_id, "PUBLISH_FAILED", result);
      return std::nullopt;
    }

    const auto [it, inserted] =
        publishers_.emplace(std::move(topic_str), std::move(publisher));
    ROSCRAFT_ASSERT(inserted, "Publisher insertion should succeed for '{}'!",
                    topic_name);
    publisher_types_.insert_or_assign(std::string(topic_name),
                                      std::move(type_str));
    publisher_qos_profiles_.insert_or_assign(std::string(topic_name),
                                             normalized_qos_profile);
    return std::ref(*it->second);
  } catch (const std::exception& ex) {
    SendError(request_id, "PUBLISH_FAILED", ex.what());
    return std::nullopt;
  } catch (...) {
    const size_t formatted_size =
        std::formatted_size("Unknown publish error for '{}'", topic_name);
    std::pmr::string result{&scratch_arena_};
    result.resize(formatted_size);
    std::format_to(result.begin(), "Unknown publish error for '{}'",
                   topic_name);

    SendError(request_id, "PUBLISH_FAILED", result);
    return std::nullopt;
  }
}

auto TopicRelayNode::EnsureMessageIntrospection(std::string_view message_type)
    -> const details::MessageIntrospection* {
  if (const auto cache_it = message_introspection_cache_.find(message_type);
      cache_it != message_introspection_cache_.end()) {
    if (!cache_it->second.has_value()) [[unlikely]] {
      return nullptr;
    }

    return &cache_it->second.value();
  }

  auto loaded = details::LoadMessageIntrospection(message_type);
  if (!loaded) {
    message_introspection_cache_.emplace(std::string(message_type),
                                         std::nullopt);
    return nullptr;
  }

  const auto [inserted_it, inserted] = message_introspection_cache_.emplace(
      std::string(message_type), std::move(*loaded));
  ROSCRAFT_ASSERT(inserted,
                  "Message introspection cache insertion should succeed for "
                  "'{}'!",
                  message_type);
  return &inserted_it->second.value();
}

auto TopicRelayNode::SerializeYamlPayload(uint64_t request_id,
                                          std::string_view topic_name,
                                          std::string_view message_type,
                                          std::span<const uint8_t> payload)
    -> std::optional<std::vector<uint8_t>> {
  const auto* introspection = EnsureMessageIntrospection(message_type);
  if (introspection == nullptr) [[unlikely]] {
    const size_t formatted_size = std::formatted_size(
        "Failed to load message introspection for type '{}'", message_type);
    std::pmr::string result{&scratch_arena_};
    result.resize(formatted_size);
    std::format_to(result.begin(),
                   "Failed to load message introspection for type '{}'",
                   message_type);

    SendError(request_id, "PUBLISH_FAILED", result);
    return std::nullopt;
  }

  const std::string_view payload_text(
      std::launder(reinterpret_cast<const char*>(payload.data())),
      payload.size());
  auto serialized = details::SerializeYamlToCdr(payload_text, *introspection);
  if (!serialized) [[unlikely]] {
    const size_t formatted_size = std::formatted_size(
        "Failed to convert YAML payload for '{}' ({}) type '{}': {}",
        topic_name, request_id, message_type, serialized.error().message);
    std::pmr::string result{&scratch_arena_};
    result.resize(formatted_size);
    std::format_to(result.begin(),
                   "Failed to convert YAML payload for '{}' ({}) type '{}': {}",
                   topic_name, request_id, message_type,
                   serialized.error().message);

    SendError(request_id, "PUBLISH_FAILED", result);
    return std::nullopt;
  }

  return std::move(*serialized);
}

bool TopicRelayNode::PublishSerializedMessage(uint64_t request_id,
                                              std::string_view topic_name,
                                              std::string_view message_type,
                                              std::span<const uint8_t> payload,
                                              std::string_view qos_profile) {
  const auto publisher =
      EnsurePublisher(topic_name, request_id, message_type, qos_profile);
  if (!publisher.has_value()) [[unlikely]] {
    return false;
  }

  rclcpp::SerializedMessage serialized_message(payload.size());
  auto& rcl_message = serialized_message.get_rcl_serialized_message();
  ROSCRAFT_ASSERT(
      serialized_message.capacity() >= payload.size(),
      "Serialized message capacity '{}' is below payload size '{}'!",
      serialized_message.capacity(), payload.size());
  if (!payload.empty()) {
    std::ranges::copy(payload, rcl_message.buffer);
  }
  rcl_message.buffer_length = payload.size();

  try {
    publisher->get().publish(serialized_message);
    return true;
  } catch (const std::exception& ex) {
    SendError(request_id, "PUBLISH_FAILED", ex.what());
  } catch (...) {
    SendError(request_id, "PUBLISH_FAILED", "Unknown publish failure");
  }

  return false;
}

void TopicRelayNode::Publish(const TopicPublishMessageCmd& cmd) {
  if (cmd.topic_name.empty() || cmd.message_type.empty()) [[unlikely]] {
    SendError(cmd.request_id, "PUBLISH_FAILED",
              "Topic name and message type must be non-empty");
    return;
  }

  if (cmd.rate_hz < 0.0) [[unlikely]] {
    SendError(cmd.request_id, "PUBLISH_FAILED", "rate_hz must be >= 0.0");
    return;
  }

  ClearPublishTimer(cmd.request_id);

  uint32_t times = cmd.times;
  if (cmd.once) {
    times = 1;
  }
  if (times == 0 && cmd.rate_hz <= 0.0) {
    times = 1;
  }

  auto qos_profile = NormalizeQosProfile(cmd.qos_profile, &scratch_arena_);
  auto converted_payload = SerializeYamlPayload(
      cmd.request_id, cmd.topic_name, cmd.message_type,
      std::span<const uint8_t>(cmd.payload.data(), cmd.payload.size()));
  if (!converted_payload.has_value()) {
    return;
  }

  if (!PublishSerializedMessage(cmd.request_id, cmd.topic_name,
                                cmd.message_type, *converted_payload,
                                qos_profile)) {
    return;
  }

  const bool finite = times > 0;
  if (finite && times <= 1) {
    return;
  }

  double rate_hz = cmd.rate_hz;
  if (rate_hz <= 0.0) {
    rate_hz = 1.0;
  }

  auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::duration<double>(1.0 / rate_hz));
  if (period <= std::chrono::nanoseconds::zero()) {
    period = std::chrono::milliseconds(1);
  }

  std::vector<uint8_t> payload(std::move(*converted_payload));
  auto remaining = std::make_shared<uint32_t>(finite ? (times - 1) : 0U);

  auto timer = this->create_wall_timer(
      period,
      [this, request_id = cmd.request_id,
       topic_name = std::string(cmd.topic_name),
       message_type = std::string(cmd.message_type),
       payload = std::move(payload), qos_profile = std::string(qos_profile),
       finite, remaining = std::move(remaining)]() {
        if (!PublishSerializedMessage(request_id, topic_name, message_type,
                                      payload, qos_profile)) {
          ClearPublishTimer(request_id);
          return;
        }

        if (!finite) {
          return;
        }

        if (*remaining == 0) {
          ClearPublishTimer(request_id);
          return;
        }

        --(*remaining);
        if (*remaining == 0) {
          ClearPublishTimer(request_id);
        }
      });
  publish_timers_[cmd.request_id] = std::move(timer);
}

void TopicRelayNode::ClearPublishTimer(uint64_t request_id) {
  const auto it = publish_timers_.find(request_id);
  if (it == publish_timers_.end()) [[unlikely]] {
    return;
  }

  it->second->cancel();
  publish_timers_.erase(it);
}

void TopicRelayNode::SendError(uint64_t request_id, std::string_view error_code,
                               std::string_view error_message) {
  ErrorCmd cmd(allocator_);
  cmd.request_id = request_id;
  cmd.error_code = std::pmr::string(error_code, allocator_);
  cmd.error_message = std::pmr::string(error_message, allocator_);
  outgoing_.get().Enqueue(error_producer_, std::move(cmd));
}

}  // namespace roscraft::bridge
