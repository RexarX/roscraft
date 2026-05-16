#pragma once

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/nodes/ros/details/introspection_codec.hpp>
#include <roscraft/memory/arena_allocator.hpp>
#include <roscraft/utils/string_hash.hpp>

#include <rclcpp/generic_subscription.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace roscraft::bridge {

struct TopicHzCmd;
struct TopicBwCmd;
struct TopicDelayCmd;

/// @brief A node that collects and reports statistics about topics.
class TopicStatsNode final : public rclcpp::Node {
public:
  /// @brief Constructs a `TopicStatsNode` with the given incoming and outgoing
  /// command queues.
  /// @param allocator The memory resource for command allocation (default:
  /// `std::pmr::get_default_resource()`)
  TopicStatsNode(
      CommandQueue& incoming, CommandQueue& outgoing,
      std::pmr::memory_resource* allocator = std::pmr::get_default_resource());

  TopicStatsNode(CommandQueue& incoming, CommandQueue& outgoing,
                 std::nullptr_t) = delete;

  TopicStatsNode(const TopicStatsNode&) = delete;
  TopicStatsNode(TopicStatsNode&&) = delete;
  ~TopicStatsNode() override = default;

  TopicStatsNode& operator=(const TopicStatsNode&) = delete;
  TopicStatsNode& operator=(TopicStatsNode&&) = delete;

private:
  /// @brief Hz/bandwidth session data.
  struct HzBwData {
    std::deque<int64_t> timestamps_ns;
    std::deque<size_t> message_sizes;
    bool is_hz = false;
    bool wall_time = false;
  };

  /// @brief Delay session data.
  struct DelayData {
    std::deque<double> delays_seconds;
  };

  struct StatsSession {
    uint64_t request_id = 0;
    std::string topic_name;
    std::string message_type;
    uint32_t window = 10;
    std::variant<HzBwData, DelayData> data;
    rclcpp::GenericSubscription::SharedPtr subscription;
  };

  void DrainHzCommands();
  void DrainBwCommands();
  void DrainDelayCommands();
  void DrainStopAllCommands();
  void StartHzSession(const TopicHzCmd& cmd);
  void StartBwSession(const TopicBwCmd& cmd);
  void StartDelaySession(const TopicDelayCmd& cmd);
  void StopSession(std::string_view session_key);
  void StopAllSessions();
  void StopSessionsByMode(std::string_view mode_suffix);

  [[nodiscard]] auto CreateSubscription(uint64_t request_id,
                                        const std::string& topic_name,
                                        const std::string& message_type,
                                        std::string session_key)
      -> rclcpp::GenericSubscription::SharedPtr;

  void HandleMessage(std::string_view session_key,
                     const rclcpp::SerializedMessage& msg);

  void OnReportTimer();
  void ReportStats();
  void SendError(uint64_t request_id, std::string_view error_code,
                 std::string_view error_message);

  [[nodiscard]] auto ExtractHeaderStampSeconds(
      std::string_view message_type, const rclcpp::SerializedMessage& msg)
      -> std::optional<double>;

  [[nodiscard]] auto EnsureDelayStampExtractor(std::string_view message_type)
      -> const details::DelayStampExtractor*;

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken hz_consumer_;
  CommandQueueConsumerToken bw_consumer_;
  CommandQueueConsumerToken delay_consumer_;
  CommandQueueConsumerToken stop_all_consumer_;
  CommandQueueProducerToken hz_response_producer_;
  CommandQueueProducerToken bw_response_producer_;
  CommandQueueProducerToken delay_response_producer_;
  CommandQueueProducerToken error_producer_;

  rclcpp::TimerBase::SharedPtr drain_timer_;
  rclcpp::TimerBase::SharedPtr report_timer_;

  std::unordered_map<std::string, StatsSession, utils::StringHash,
                     utils::StringEqual>
      sessions_;

  std::unordered_map<std::string, std::optional<details::DelayStampExtractor>,
                     utils::StringHash, utils::StringEqual>
      delay_stamp_extractors_;

  std::pmr::memory_resource* allocator_ = std::pmr::get_default_resource();
  memory::ArenaAllocator scratch_arena_{128 * 1024};
};

}  // namespace roscraft::bridge
