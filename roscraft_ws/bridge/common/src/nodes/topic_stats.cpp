#include <pch.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/nodes/topic_stats.hpp>

#include <rclcpp/logging.hpp>
#include <rclcpp/qos.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/serialized_message.hpp>

#include <chrono>
#include <cstdint>
#include <format>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

TopicStatsNode::TopicStatsNode(CommandQueue& incoming, CommandQueue& outgoing)
    : rclcpp::Node("roscraft_topic_stats_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      hz_consumer_(incoming.MakeConsumerToken<TopicHzCmd>()),
      bw_consumer_(incoming.MakeConsumerToken<TopicBwCmd>()),
      hz_response_producer_(outgoing.MakeProducerToken<TopicHzResponseCmd>()),
      bw_response_producer_(outgoing.MakeProducerToken<TopicBwResponseCmd>()),
      error_producer_(outgoing.MakeProducerToken<ErrorCmd>()) {
  using namespace std::chrono_literals;
  drain_timer_ = this->create_wall_timer(100ms, [this] {
    DrainHzCommands();
    DrainBwCommands();
  });
  report_timer_ = this->create_wall_timer(1s, [this] { OnReportTimer(); });
}

void TopicStatsNode::DrainHzCommands() {
  auto& storage = incoming_.get().TypedStorage<TopicHzCmd>();
  TopicHzCmd cmd(std::pmr::get_default_resource());
  while (storage.Dequeue(hz_consumer_, cmd)) {
    StartHzSession(cmd);
  }
}

void TopicStatsNode::DrainBwCommands() {
  auto& storage = incoming_.get().TypedStorage<TopicBwCmd>();
  TopicBwCmd cmd(std::pmr::get_default_resource());
  while (storage.Dequeue(bw_consumer_, cmd)) {
    StartBwSession(cmd);
  }
}

void TopicStatsNode::StartHzSession(const TopicHzCmd& cmd) {
  if (cmd.topic_name.empty() || cmd.message_type.empty()) [[unlikely]] {
    SendError(cmd.request_id, "TOPIC_HZ_FAILED",
              "Topic name and message type must be non-empty");
    return;
  }

  const auto it = sessions_.find(cmd.topic_name);
  if (it != sessions_.end()) {
    auto& session = it->second;
    if (session.message_type != std::string_view(cmd.message_type))
        [[unlikely]] {
      SendError(
          cmd.request_id, "TOPIC_HZ_FAILED",
          std::format("Topic '{}' already has stats session with type '{}'",
                      cmd.topic_name, session.message_type));
      return;
    }
    session.request_id = cmd.request_id;
    session.window = cmd.window;
    session.is_hz = true;
    session.timestamps.clear();
    session.message_sizes.clear();
    RCLCPP_INFO(this->get_logger(),
                "Reset hz session for '%s' (request %lu, window %u)",
                cmd.topic_name.c_str(),
                static_cast<unsigned long>(cmd.request_id), cmd.window);
    return;
  }

  auto sub =
      CreateSubscription(cmd.topic_name, cmd.request_id, cmd.message_type);
  if (!sub) {
    return;
  };

  StatsSession session{
      .request_id = cmd.request_id,
      .topic_name = std::string(cmd.topic_name),
      .message_type = std::string(cmd.message_type),
      .window = cmd.window == 0 ? 10 : cmd.window,
      .is_hz = true,
      .timestamps = {},
      .message_sizes = {},
      .subscription = std::move(sub),
  };
  sessions_.emplace(std::string(cmd.topic_name), std::move(session));

  RCLCPP_INFO(this->get_logger(),
              "Started hz session for '%s' (request %lu, window %u)",
              cmd.topic_name.c_str(),
              static_cast<unsigned long>(cmd.request_id), cmd.window);
}

void TopicStatsNode::StartBwSession(const TopicBwCmd& cmd) {
  if (cmd.topic_name.empty() || cmd.message_type.empty()) [[unlikely]] {
    SendError(cmd.request_id, "TOPIC_BW_FAILED",
              "Topic name and message type must be non-empty");
    return;
  }

  const auto it = sessions_.find(cmd.topic_name);
  if (it != sessions_.end()) {
    auto& session = it->second;
    if (session.message_type != std::string_view(cmd.message_type))
        [[unlikely]] {
      SendError(
          cmd.request_id, "TOPIC_BW_FAILED",
          std::format("Topic '{}' already has stats session with type '{}'",
                      cmd.topic_name, session.message_type));
      return;
    }
    session.request_id = cmd.request_id;
    session.window = cmd.window;
    session.is_hz = false;
    session.timestamps.clear();
    session.message_sizes.clear();
    RCLCPP_INFO(this->get_logger(),
                "Reset bw session for '%s' (request %lu, window %u)",
                cmd.topic_name.c_str(),
                static_cast<unsigned long>(cmd.request_id), cmd.window);
    return;
  }

  auto sub =
      CreateSubscription(cmd.topic_name, cmd.request_id, cmd.message_type);
  if (!sub) {
    return;
  }

  StatsSession session{
      .request_id = cmd.request_id,
      .topic_name = std::string(cmd.topic_name),
      .message_type = std::string(cmd.message_type),
      .window = cmd.window == 0 ? 10 : cmd.window,
      .is_hz = false,
      .timestamps = {},
      .message_sizes = {},
      .subscription = std::move(sub),
  };
  sessions_.emplace(std::string(cmd.topic_name), std::move(session));

  RCLCPP_INFO(this->get_logger(),
              "Started bw session for '%s' (request %lu, window %u)",
              cmd.topic_name.c_str(),
              static_cast<unsigned long>(cmd.request_id), cmd.window);
}

auto TopicStatsNode::CreateSubscription(std::string_view topic_name,
                                        uint64_t request_id,
                                        std::string_view message_type)
    -> rclcpp::GenericSubscription::SharedPtr {
  try {
    auto subscription = this->create_generic_subscription(
        std::string(topic_name), std::string(message_type), rclcpp::QoS(10),
        [this, topic = std::string(topic_name)](
            std::shared_ptr<rclcpp::SerializedMessage> msg) {
          HandleMessage(topic, *msg);
        });

    if (subscription == nullptr) [[unlikely]] {
      SendError(
          request_id, "STATS_FAILED",
          std::format("Failed to create subscription for '{}'", topic_name));
      return nullptr;
    }

    return subscription;
  } catch (const std::exception& ex) {
    SendError(request_id, "STATS_FAILED", ex.what());
    return nullptr;
  } catch (...) {
    SendError(request_id, "STATS_FAILED",
              std::format("Unknown subscribe error for '{}'", topic_name));
    return nullptr;
  }
}

void TopicStatsNode::HandleMessage(std::string_view topic_name,
                                   const rclcpp::SerializedMessage& msg) {
  const auto it = sessions_.find(topic_name);
  if (it == sessions_.end()) [[unlikely]] {
    return;
  }

  auto& session = it->second;
  const auto now = std::chrono::steady_clock::now();
  const auto& serialized = msg.get_rcl_serialized_message();
  const size_t msg_size = serialized.buffer_length;

  session.timestamps.push_back(now);
  session.message_sizes.push_back(msg_size);

  while (session.timestamps.size() > session.window) {
    session.timestamps.pop_front();
    session.message_sizes.pop_front();
  }
}

void TopicStatsNode::OnReportTimer() {
  ReportStats();
}

void TopicStatsNode::ReportStats() {
  std::vector<std::string> to_remove;

  for (auto& [topic_name, session] : sessions_) {
    if (session.timestamps.size() < 2) {
      continue;
    }

    if (session.is_hz) {
      const auto& front = session.timestamps.front();
      const auto& back = session.timestamps.back();
      const double elapsed =
          std::chrono::duration<double>(back - front).count();
      if (elapsed <= 0.0) [[unlikely]] {
        continue;
      }

      const auto frequency =
          static_cast<double>(session.timestamps.size() - 1) / elapsed;

      TopicHzResponseCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = session.request_id;
      cmd.topic_name = std::pmr::string(session.topic_name,
                                        std::pmr::get_default_resource());
      cmd.frequency = frequency;
      cmd.window = session.window;
      cmd.message_count = static_cast<uint32_t>(session.timestamps.size());

      outgoing_.get().Enqueue(hz_response_producer_, std::move(cmd));
    } else {
      const auto& front = session.timestamps.front();
      const auto& back = session.timestamps.back();
      const double elapsed =
          std::chrono::duration<double>(back - front).count();
      if (elapsed <= 0.0) [[unlikely]] {
        continue;
      }

      uint64_t total_bytes = 0;
      for (const auto size : session.message_sizes) {
        total_bytes += size;
      }

      const auto bytes_per_second = static_cast<double>(total_bytes) / elapsed;

      TopicBwResponseCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = session.request_id;
      cmd.topic_name = std::pmr::string(session.topic_name,
                                        std::pmr::get_default_resource());
      cmd.bytes_per_second = bytes_per_second;
      cmd.window = session.window;
      cmd.message_count = static_cast<uint32_t>(session.timestamps.size());
      cmd.total_bytes = total_bytes;

      outgoing_.get().Enqueue(bw_response_producer_, std::move(cmd));
    }

    session.timestamps.clear();
    session.message_sizes.clear();
  }

  for (const auto& name : to_remove) {
    sessions_.erase(name);
  }
}

void TopicStatsNode::SendError(uint64_t request_id, std::string_view error_code,
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
