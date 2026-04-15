#pragma once

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/utils/string_hash.hpp>

#include <rclcpp/generic_subscription.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace roscraft::bridge {

/// @brief A node that collects and reports statistics about topics.
class TopicStatsNode final : public rclcpp::Node {
public:
  /// @brief Constructs a `TopicStatsNode` with the given incoming and outgoing
  /// command queues.
  TopicStatsNode(CommandQueue& incoming, CommandQueue& outgoing);
  TopicStatsNode(const TopicStatsNode&) = delete;
  TopicStatsNode(TopicStatsNode&&) = delete;
  ~TopicStatsNode() override = default;

  TopicStatsNode& operator=(const TopicStatsNode&) = delete;
  TopicStatsNode& operator=(TopicStatsNode&&) = delete;

private:
  struct StatsSession {
    uint64_t request_id = 0;
    std::string topic_name;
    std::string message_type;
    uint32_t window = 10;
    bool is_hz = false;
    std::deque<std::chrono::steady_clock::time_point> timestamps;
    std::deque<size_t> message_sizes;
    rclcpp::GenericSubscription::SharedPtr subscription;
  };

  void DrainHzCommands();
  void DrainBwCommands();
  void StartHzSession(const TopicHzCmd& cmd);
  void StartBwSession(const TopicBwCmd& cmd);

  [[nodiscard]] auto CreateSubscription(std::string_view topic_name,
                                        uint64_t request_id,
                                        std::string_view message_type)
      -> rclcpp::GenericSubscription::SharedPtr;

  void HandleMessage(std::string_view topic_name,
                     const rclcpp::SerializedMessage& msg);

  void OnReportTimer();
  void ReportStats();
  void SendError(uint64_t request_id, std::string_view error_code,
                 std::string_view error_message);

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken hz_consumer_;
  CommandQueueConsumerToken bw_consumer_;
  CommandQueueProducerToken hz_response_producer_;
  CommandQueueProducerToken bw_response_producer_;
  CommandQueueProducerToken error_producer_;

  rclcpp::TimerBase::SharedPtr drain_timer_;
  rclcpp::TimerBase::SharedPtr report_timer_;

  std::unordered_map<std::string, StatsSession, utils::StringHash,
                     utils::StringEqual>
      sessions_;
};

}  // namespace roscraft::bridge
