#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/topic/stats.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/error.hpp>
#include <roscraft/bridge/command/types/topic.hpp>
#include <roscraft/bridge/nodes/ros/details/introspection_codec.hpp>
#include <roscraft/memory/arena_allocator.hpp>

#include <rclcpp/logging.hpp>
#include <rclcpp/qos.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/serialized_message.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace roscraft::bridge {

namespace {

[[nodiscard]] std::pmr::string BuildSessionKey(
    std::string_view topic_name, std::string_view mode_suffix,
    std::pmr::memory_resource* allocator = std::pmr::get_default_resource()) {
  std::pmr::string key{allocator};
  const size_t formatted_size =
      std::formatted_size("{}#{}", topic_name, mode_suffix);
  key.resize(formatted_size);
  std::format_to(key.begin(), "{}#{}", topic_name, mode_suffix);

  return key;
}

[[nodiscard]] constexpr uint32_t SafeWindow(uint32_t window) noexcept {
  return window == 0 ? 0U : window;
}

[[nodiscard]] int64_t ToNanoseconds(const rclcpp::Time& time_point) noexcept {
  return time_point.nanoseconds();
}

[[nodiscard]] constexpr int64_t ToNanoseconds(
    const std::chrono::steady_clock::time_point& time_point) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             time_point.time_since_epoch())
      .count();
}

[[nodiscard]] constexpr double ElapsedSecondsFromNanoseconds(
    int64_t first_ns, int64_t last_ns) noexcept {
  if (last_ns <= first_ns) [[unlikely]] {
    return 0.0;
  }

  constexpr double kNanosPerSecond = 1'000'000'000.0;
  return static_cast<double>(last_ns - first_ns) / kNanosPerSecond;
}

[[nodiscard]] auto InferTopicType(rclcpp::Node& node,
                                  std::string_view topic_name)
    -> std::optional<std::string> {
  const auto topic_map = node.get_topic_names_and_types();
  const auto topic_it = topic_map.find(std::string(topic_name));
  if (topic_it == topic_map.end() || topic_it->second.empty()) {
    return std::nullopt;
  }

  return topic_it->second.front();
}

}  // namespace

TopicStatsNode::TopicStatsNode(CommandQueue& incoming, CommandQueue& outgoing,
                               std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_topic_stats_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      hz_consumer_(incoming.MakeConsumerToken<TopicHzCmd>()),
      bw_consumer_(incoming.MakeConsumerToken<TopicBwCmd>()),
      delay_consumer_(incoming.MakeConsumerToken<TopicDelayCmd>()),
      stop_all_consumer_(incoming.MakeConsumerToken<TopicStatsStopAllCmd>()),
      hz_response_producer_(outgoing.MakeProducerToken<TopicHzResponseCmd>()),
      bw_response_producer_(outgoing.MakeProducerToken<TopicBwResponseCmd>()),
      delay_response_producer_(
          outgoing.MakeProducerToken<TopicDelayResponseCmd>()),
      error_producer_(outgoing.MakeProducerToken<ErrorCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  drain_timer_ = this->create_wall_timer(50ms, [this] {
    DrainHzCommands();
    DrainBwCommands();
    DrainDelayCommands();
    DrainStopAllCommands();
    scratch_arena_.Reset();
  });
  report_timer_ = this->create_wall_timer(1s, [this] {
    OnReportTimer();
    scratch_arena_.Reset();
  });
}

void TopicStatsNode::DrainHzCommands() {
  auto& storage = incoming_.get().TypedStorage<TopicHzCmd>();

  TopicHzCmd cmd(allocator_);
  while (storage.Dequeue(hz_consumer_, cmd)) {
    StartHzSession(cmd);
  }
}

void TopicStatsNode::DrainBwCommands() {
  auto& storage = incoming_.get().TypedStorage<TopicBwCmd>();

  TopicBwCmd cmd(allocator_);
  while (storage.Dequeue(bw_consumer_, cmd)) {
    StartBwSession(cmd);
  }
}

void TopicStatsNode::DrainDelayCommands() {
  auto& storage = incoming_.get().TypedStorage<TopicDelayCmd>();

  TopicDelayCmd cmd(allocator_);
  while (storage.Dequeue(delay_consumer_, cmd)) {
    StartDelaySession(cmd);
  }
}

void TopicStatsNode::DrainStopAllCommands() {
  auto& storage = incoming_.get().TypedStorage<TopicStatsStopAllCmd>();

  TopicStatsStopAllCmd cmd;
  while (storage.Dequeue(stop_all_consumer_, cmd)) {
    StopAllSessions();
  }
}

void TopicStatsNode::StartHzSession(const TopicHzCmd& cmd) {
  if (cmd.topic_name.empty()) [[unlikely]] {
    if (cmd.window == 0) {
      StopSessionsByMode("#hz");
      return;
    }
    SendError(cmd.request_id, "TOPIC_HZ_FAILED",
              "Topic name must be non-empty");
    return;
  }

  const auto session_key =
      BuildSessionKey(cmd.topic_name, "hz", &scratch_arena_);

  if (cmd.window == 0) {
    StopSession(session_key);
    return;
  }

  const auto it = sessions_.find(session_key);

  std::string resolved_message_type;
  if (cmd.message_type.empty()) {
    if (it != sessions_.end()) {
      resolved_message_type = it->second.message_type;
    } else {
      const auto inferred = InferTopicType(*this, cmd.topic_name);
      if (!inferred.has_value()) [[unlikely]] {
        const size_t formatted_size = std::formatted_size(
            "Unable to infer message type for topic '{}'", cmd.topic_name);
        std::pmr::string result{&scratch_arena_};
        result.resize(formatted_size);
        std::format_to(result.begin(),
                       "Unable to infer message type for topic '{}'",
                       cmd.topic_name);

        SendError(cmd.request_id, "TOPIC_HZ_FAILED", result);
        return;
      }

      resolved_message_type = *inferred;
    }
  } else {
    resolved_message_type.assign(cmd.message_type);
  }

  if (it != sessions_.end()) {
    auto& session = it->second;
    if (session.message_type != resolved_message_type) [[unlikely]] {
      const size_t formatted_size = std::formatted_size(
          "Topic '{}' already has stats session with type '{}'", cmd.topic_name,
          session.message_type);
      std::pmr::string result{&scratch_arena_};
      result.resize(formatted_size);
      std::format_to(result.begin(),
                     "Topic '{}' already has stats session with type '{}'",
                     cmd.topic_name, session.message_type);

      SendError(cmd.request_id, "TOPIC_HZ_FAILED", result);
      return;
    }
    session.request_id = cmd.request_id;
    session.window = SafeWindow(cmd.window);
    session.data = HzBwData{.timestamps_ns = {},
                            .message_sizes = {},
                            .is_hz = true,
                            .wall_time = cmd.wall_time};
    RCLCPP_INFO(this->get_logger(),
                "Reset hz session for '%s' (request %lu, window %u)",
                cmd.topic_name.c_str(),
                static_cast<unsigned long>(cmd.request_id), cmd.window);
    return;
  }

  auto sub =
      CreateSubscription(cmd.request_id, std::string(cmd.topic_name),
                         resolved_message_type, std::string(session_key));
  if (!sub) {
    return;
  };

  StatsSession session{
      .request_id = cmd.request_id,
      .topic_name = std::string(cmd.topic_name),
      .message_type = std::move(resolved_message_type),
      .window = SafeWindow(cmd.window),
      .data = HzBwData{.timestamps_ns = {},
                       .message_sizes = {},
                       .is_hz = true,
                       .wall_time = cmd.wall_time},
      .subscription = std::move(sub),
  };
  sessions_.emplace(session_key, std::move(session));

  RCLCPP_INFO(this->get_logger(),
              "Started hz session for '%s' (request %lu, window %u)",
              cmd.topic_name.c_str(),
              static_cast<unsigned long>(cmd.request_id), cmd.window);
}

void TopicStatsNode::StartBwSession(const TopicBwCmd& cmd) {
  if (cmd.topic_name.empty()) [[unlikely]] {
    if (cmd.window == 0) {
      StopSessionsByMode("#bw");
      return;
    }
    SendError(cmd.request_id, "TOPIC_BW_FAILED",
              "Topic name must be non-empty");
    return;
  }

  const auto session_key =
      BuildSessionKey(cmd.topic_name, "bw", &scratch_arena_);

  if (cmd.window == 0) {
    StopSession(session_key);
    return;
  }

  const auto it = sessions_.find(session_key);

  std::string resolved_message_type;
  if (cmd.message_type.empty()) {
    if (it != sessions_.end()) {
      resolved_message_type = it->second.message_type;
    } else {
      const auto inferred = InferTopicType(*this, cmd.topic_name);
      if (!inferred.has_value()) [[unlikely]] {
        const size_t formatted_size = std::formatted_size(
            "Unable to infer message type for topic '{}'", cmd.topic_name);
        std::pmr::string result{&scratch_arena_};
        result.resize(formatted_size);
        std::format_to(result.begin(),
                       "Unable to infer message type for topic '{}'",
                       cmd.topic_name);

        SendError(cmd.request_id, "TOPIC_BW_FAILED", result);
        return;
      }

      resolved_message_type = *inferred;
    }
  } else {
    resolved_message_type.assign(cmd.message_type);
  }

  if (it != sessions_.end()) {
    auto& session = it->second;
    if (session.message_type != resolved_message_type) [[unlikely]] {
      const size_t formatted_size = std::formatted_size(
          "Topic '{}' already has stats session with type '{}'", cmd.topic_name,
          session.message_type);
      std::pmr::string result{&scratch_arena_};
      result.resize(formatted_size);
      std::format_to(result.begin(),
                     "Topic '{}' already has stats session with type '{}'",
                     cmd.topic_name, session.message_type);

      SendError(cmd.request_id, "TOPIC_BW_FAILED", result);
      return;
    }
    session.request_id = cmd.request_id;
    session.window = SafeWindow(cmd.window);
    session.data = HzBwData{.timestamps_ns = {},
                            .message_sizes = {},
                            .is_hz = false,
                            .wall_time = cmd.wall_time};
    RCLCPP_INFO(this->get_logger(),
                "Reset bw session for '%s' (request %lu, window %u)",
                cmd.topic_name.c_str(),
                static_cast<unsigned long>(cmd.request_id), cmd.window);
    return;
  }

  auto sub =
      CreateSubscription(cmd.request_id, std::string(cmd.topic_name),
                         resolved_message_type, std::string(session_key));
  if (!sub) {
    return;
  }

  StatsSession session{
      .request_id = cmd.request_id,
      .topic_name = std::string(cmd.topic_name),
      .message_type = std::move(resolved_message_type),
      .window = SafeWindow(cmd.window),
      .data = HzBwData{.timestamps_ns = {},
                       .message_sizes = {},
                       .is_hz = false,
                       .wall_time = cmd.wall_time},
      .subscription = std::move(sub),
  };
  sessions_.emplace(session_key, std::move(session));

  RCLCPP_INFO(this->get_logger(),
              "Started bw session for '%s' (request %lu, window %u)",
              cmd.topic_name.c_str(),
              static_cast<unsigned long>(cmd.request_id), cmd.window);
}

void TopicStatsNode::StartDelaySession(const TopicDelayCmd& cmd) {
  if (cmd.topic_name.empty()) [[unlikely]] {
    if (cmd.window == 0) {
      StopSessionsByMode("#delay");
      return;
    }
    SendError(cmd.request_id, "TOPIC_DELAY_FAILED",
              "Topic name must be non-empty");
    return;
  }

  const auto session_key =
      BuildSessionKey(cmd.topic_name, "delay", &scratch_arena_);

  if (cmd.window == 0) {
    StopSession(session_key);
    return;
  }

  const auto it = sessions_.find(session_key);

  std::string resolved_message_type;
  if (cmd.message_type.empty()) {
    if (it != sessions_.end()) {
      resolved_message_type = it->second.message_type;
    } else {
      const auto inferred = InferTopicType(*this, cmd.topic_name);
      if (!inferred.has_value()) [[unlikely]] {
        const size_t formatted_size = std::formatted_size(
            "Unable to infer message type for topic '{}'", cmd.topic_name);
        std::pmr::string result{&scratch_arena_};
        result.resize(formatted_size);
        std::format_to(result.begin(),
                       "Unable to infer message type for topic '{}'",
                       cmd.topic_name);

        SendError(cmd.request_id, "TOPIC_DELAY_FAILED", result);
        return;
      }

      resolved_message_type = *inferred;
    }
  } else {
    resolved_message_type.assign(cmd.message_type);
  }

  if (EnsureDelayStampExtractor(resolved_message_type) == nullptr)
      [[unlikely]] {
    const size_t formatted_size = std::formatted_size(
        "Unsupported delay message type '{}' (requires header.stamp or "
        "builtin_interfaces/msg/Time layout)",
        resolved_message_type);
    std::pmr::string result{&scratch_arena_};
    result.resize(formatted_size);
    std::format_to(result.begin(),
                   "Unsupported delay message type '{}' (requires header.stamp "
                   "or builtin_interfaces/msg/Time layout)",
                   resolved_message_type);

    SendError(cmd.request_id, "TOPIC_DELAY_FAILED", result);
    return;
  }

  if (it != sessions_.end()) {
    auto& session = it->second;
    if (session.message_type != resolved_message_type) [[unlikely]] {
      const size_t formatted_size = std::formatted_size(
          "Topic '{}' already has delay session with type '{}'", cmd.topic_name,
          session.message_type);
      std::pmr::string result{&scratch_arena_};
      result.resize(formatted_size);
      std::format_to(result.begin(),
                     "Topic '{}' already has delay session with type '{}'",
                     cmd.topic_name, session.message_type);

      SendError(cmd.request_id, "TOPIC_DELAY_FAILED", result);
      return;
    }
    session.request_id = cmd.request_id;
    session.window = SafeWindow(cmd.window);
    session.data = DelayData{};
    RCLCPP_INFO(this->get_logger(),
                "Reset delay session for '%s' (request %lu, window %u)",
                cmd.topic_name.c_str(),
                static_cast<unsigned long>(cmd.request_id), cmd.window);
    return;
  }

  auto sub =
      CreateSubscription(cmd.request_id, std::string(cmd.topic_name),
                         resolved_message_type, std::string(session_key));
  if (!sub) {
    return;
  }

  StatsSession session{
      .request_id = cmd.request_id,
      .topic_name = std::string(cmd.topic_name),
      .message_type = std::move(resolved_message_type),
      .window = SafeWindow(cmd.window),
      .data = DelayData{},
      .subscription = std::move(sub),
  };
  sessions_.emplace(session_key, std::move(session));

  RCLCPP_INFO(this->get_logger(),
              "Started delay session for '%s' (request %lu, window %u)",
              cmd.topic_name.c_str(),
              static_cast<unsigned long>(cmd.request_id), cmd.window);
}

auto TopicStatsNode::CreateSubscription(uint64_t request_id,
                                        const std::string& topic_name,
                                        const std::string& message_type,
                                        std::string session_key)
    -> rclcpp::GenericSubscription::SharedPtr {
  try {
    auto subscription = this->create_generic_subscription(
        topic_name, message_type, rclcpp::QoS(10),
        [this, session_key = std::move(session_key)](
            std::shared_ptr<rclcpp::SerializedMessage> msg) {
          HandleMessage(session_key, *msg);
        });

    if (subscription == nullptr) [[unlikely]] {
      const size_t formatted_size = std::formatted_size(
          "Failed to create subscription for '{}'", topic_name);
      std::pmr::string result{&scratch_arena_};
      result.resize(formatted_size);
      std::format_to(result.begin(), "Failed to create subscription for '{}'",
                     topic_name);

      SendError(request_id, "STATS_FAILED", result);
      return nullptr;
    }

    return subscription;
  } catch (const std::exception& ex) {
    SendError(request_id, "STATS_FAILED", ex.what());
    return nullptr;
  } catch (...) {
    const size_t formatted_size =
        std::formatted_size("Unknown subscribe error for '{}'", topic_name);
    std::pmr::string result{&scratch_arena_};
    result.resize(formatted_size);
    std::format_to(result.begin(), "Unknown subscribe error for '{}'",
                   topic_name);

    SendError(request_id, "STATS_FAILED", result);
    return nullptr;
  }
}

void TopicStatsNode::HandleMessage(std::string_view session_key,
                                   const rclcpp::SerializedMessage& msg) {
  const auto it = sessions_.find(session_key);
  if (it == sessions_.end()) [[unlikely]] {
    return;
  }

  auto& session = it->second;
  const auto visitor = [&](auto& data) {
    using T = std::decay_t<decltype(data)>;
    if constexpr (std::same_as<T, HzBwData>) {
      int64_t timestamp_ns = 0;
      if (data.wall_time) {
        timestamp_ns = ToNanoseconds(std::chrono::steady_clock::now());
      } else {
        timestamp_ns = ToNanoseconds(this->get_clock()->now());
      }
      const auto& serialized = msg.get_rcl_serialized_message();
      const size_t msg_size = serialized.buffer_length;

      data.timestamps_ns.push_back(timestamp_ns);
      data.message_sizes.push_back(msg_size);

      while (data.timestamps_ns.size() > session.window) {
        data.timestamps_ns.pop_front();
        data.message_sizes.pop_front();
      }
    } else if constexpr (std::same_as<T, DelayData>) {
      const auto stamp_seconds =
          ExtractHeaderStampSeconds(session.message_type, msg);
      if (!stamp_seconds.has_value()) {
        return;
      }

      const auto now_seconds =
          std::chrono::duration<double>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
      const double delay_seconds = now_seconds - *stamp_seconds;

      data.delays_seconds.push_back(delay_seconds);
      while (data.delays_seconds.size() > session.window) {
        data.delays_seconds.pop_front();
      }
    }
  };

  std::visit(visitor, session.data);
}

void TopicStatsNode::OnReportTimer() {
  ReportStats();
}

void TopicStatsNode::ReportStats() {
  auto* const mr = std::pmr::get_default_resource();
  for (auto& [_, session] : sessions_) {
    const auto visitor = [&](auto& data) {
      using T = std::decay_t<decltype(data)>;
      if constexpr (std::same_as<T, DelayData>) {
        if (data.delays_seconds.empty()) {
          return;
        }

        double total_delay = 0.0;
        double min_delay = std::numeric_limits<double>::infinity();
        double max_delay = -std::numeric_limits<double>::infinity();
        for (const double delay : data.delays_seconds) {
          total_delay += delay;
          if (delay < min_delay) {
            min_delay = delay;
          }
          if (delay > max_delay) {
            max_delay = delay;
          }
        }

        const double average_delay =
            total_delay / static_cast<double>(data.delays_seconds.size());

        TopicDelayResponseCmd cmd(mr);
        cmd.request_id = session.request_id;
        cmd.topic_name = std::pmr::string(session.topic_name, mr);
        cmd.average_delay = average_delay;
        cmd.min_delay = min_delay;
        cmd.max_delay = max_delay;
        cmd.window = session.window;
        cmd.message_count = static_cast<uint32_t>(data.delays_seconds.size());

        outgoing_.get().Enqueue(delay_response_producer_, std::move(cmd));
      } else if constexpr (std::same_as<T, HzBwData>) {
        if (data.timestamps_ns.size() < 2) {
          return;
        }

        if (data.is_hz) {
          const int64_t first_ns = data.timestamps_ns.front();
          const int64_t last_ns = data.timestamps_ns.back();
          const double elapsed =
              ElapsedSecondsFromNanoseconds(first_ns, last_ns);
          if (elapsed <= 0.0) [[unlikely]] {
            return;
          }

          const auto frequency =
              static_cast<double>(data.timestamps_ns.size() - 1) / elapsed;

          TopicHzResponseCmd cmd(mr);
          cmd.request_id = session.request_id;
          cmd.topic_name = std::pmr::string(session.topic_name, mr);
          cmd.frequency = frequency;
          cmd.window = session.window;
          cmd.message_count = static_cast<uint32_t>(data.timestamps_ns.size());

          outgoing_.get().Enqueue(hz_response_producer_, std::move(cmd));
        } else {
          const int64_t first_ns = data.timestamps_ns.front();
          const int64_t last_ns = data.timestamps_ns.back();
          const double elapsed =
              ElapsedSecondsFromNanoseconds(first_ns, last_ns);
          if (elapsed <= 0.0) [[unlikely]] {
            return;
          }

          uint64_t total_bytes = 0;
          for (const auto size : data.message_sizes) {
            total_bytes += size;
          }

          const auto bytes_per_second =
              static_cast<double>(total_bytes) / elapsed;

          TopicBwResponseCmd cmd(mr);
          cmd.request_id = session.request_id;
          cmd.topic_name = std::pmr::string(session.topic_name, mr);
          cmd.bytes_per_second = bytes_per_second;
          cmd.window = session.window;
          cmd.message_count = static_cast<uint32_t>(data.timestamps_ns.size());
          cmd.total_bytes = total_bytes;

          outgoing_.get().Enqueue(bw_response_producer_, std::move(cmd));
        }
      }
    };
    std::visit(visitor, session.data);
  }
}

auto TopicStatsNode::ExtractHeaderStampSeconds(
    std::string_view message_type, const rclcpp::SerializedMessage& msg)
    -> std::optional<double> {
  const auto* extractor = EnsureDelayStampExtractor(message_type);
  if (extractor == nullptr) [[unlikely]] {
    return std::nullopt;
  }

  const auto& serialized = msg.get_rcl_serialized_message();
  const auto* payload = serialized.buffer;
  if (payload == nullptr && serialized.buffer_length > 0) [[unlikely]] {
    return std::nullopt;
  }

  return details::ExtractStampSecondsFromCdr(
      std::span<const uint8_t>(payload, serialized.buffer_length), *extractor);
}

auto TopicStatsNode::EnsureDelayStampExtractor(std::string_view message_type)
    -> const details::DelayStampExtractor* {
  if (const auto extractor_it = delay_stamp_extractors_.find(message_type);
      extractor_it != delay_stamp_extractors_.end()) {
    if (!extractor_it->second.has_value()) [[unlikely]] {
      return nullptr;
    }

    return &extractor_it->second.value();
  }

  auto extractor = details::LoadDelayStampExtractor(message_type);
  if (!extractor) {
    delay_stamp_extractors_.emplace(std::string(message_type), std::nullopt);
    return nullptr;
  }

  const auto [inserted_it, inserted] = delay_stamp_extractors_.emplace(
      std::string(message_type), std::move(*extractor));
  ROSCRAFT_ASSERT(inserted,
                  "Delay stamp extractor insertion should succeed for '{}'!",
                  message_type);
  return &inserted_it->second.value();
}

void TopicStatsNode::StopSession(std::string_view session_key) {
  const auto it = sessions_.find(session_key);
  if (it == sessions_.end()) [[unlikely]] {
    return;
  }

  RCLCPP_INFO(this->get_logger(), "Stopped stats session for '%s'",
              it->second.topic_name.c_str());
  sessions_.erase(it);
}

void TopicStatsNode::StopAllSessions() {
  if (sessions_.empty()) {
    return;
  }

  RCLCPP_INFO(this->get_logger(), "Stopping all stats sessions (%zu total)",
              sessions_.size());
  sessions_.clear();
}

void TopicStatsNode::StopSessionsByMode(std::string_view mode_suffix) {
  size_t stopped_count = 0;
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if (it->first.size() > mode_suffix.size() &&
        it->first.ends_with(mode_suffix)) {
      it = sessions_.erase(it);
      ++stopped_count;
    } else {
      ++it;
    }
  }

  if (stopped_count > 0) {
    RCLCPP_INFO(this->get_logger(),
                "Stopped %zu stats session(s) for mode '%.*s'", stopped_count,
                static_cast<int>(mode_suffix.size()), mode_suffix.data());
  }
}

void TopicStatsNode::SendError(uint64_t request_id, std::string_view error_code,
                               std::string_view error_message) {
  ErrorCmd cmd(allocator_);
  cmd.request_id = request_id;
  cmd.error_code = std::pmr::string(error_code, allocator_);
  cmd.error_message = std::pmr::string(error_message, allocator_);
  outgoing_.get().Enqueue(error_producer_, std::move(cmd));
}

}  // namespace roscraft::bridge
