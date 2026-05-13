#pragma once

#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

/// @brief Query detailed information about a specific topic.
struct TopicInfoCmd {
  static constexpr std::string_view kName = "TopicInfoCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;

  TopicInfoCmd() : TopicInfoCmd(std::pmr::get_default_resource()) {}
  explicit TopicInfoCmd(std::pmr::memory_resource* mr) : topic_name(mr) {}
};

/// @brief Ask the bridge to start forwarding a topic's messages to the mod.
struct TopicSubscribeCmd {
  static constexpr std::string_view kName = "TopicSubscribeCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  std::pmr::string message_type;  ///< e.g. "geometry_msgs/msg/Twist"
  double timeout_seconds = 0.0;
  bool once = false;
  bool raw = false;

  TopicSubscribeCmd() : TopicSubscribeCmd(std::pmr::get_default_resource()) {}
  explicit TopicSubscribeCmd(std::pmr::memory_resource* mr)
      : topic_name(mr), message_type(mr) {}
};

/// @brief Ask the bridge to stop forwarding a topic's messages to the mod.
struct TopicUnsubscribeCmd {
  static constexpr std::string_view kName = "TopicUnsubscribeCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;

  TopicUnsubscribeCmd()
      : TopicUnsubscribeCmd(std::pmr::get_default_resource()) {}
  explicit TopicUnsubscribeCmd(std::pmr::memory_resource* mr)
      : topic_name(mr) {}
};

/// @brief Publish a message onto a ROS topic from UTF-8 YAML text.
struct TopicPublishMessageCmd {
  static constexpr std::string_view kName = "TopicPublishMessageCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  std::pmr::string message_type;
  std::pmr::vector<uint8_t> payload;  ///< UTF-8 YAML payload bytes
  bool once = false;
  double rate_hz = 0.0;
  uint32_t times = 0;
  std::pmr::string qos_profile;

  TopicPublishMessageCmd()
      : TopicPublishMessageCmd(std::pmr::get_default_resource()) {}
  explicit TopicPublishMessageCmd(std::pmr::memory_resource* mr)
      : topic_name(mr), message_type(mr), payload(mr), qos_profile(mr) {}
};

/// @brief Request topic frequency (hz) measurement.
struct TopicHzCmd {
  static constexpr std::string_view kName = "TopicHzCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  std::pmr::string message_type;
  uint32_t window = 10;
  bool wall_time = false;

  TopicHzCmd() : TopicHzCmd(std::pmr::get_default_resource()) {}
  explicit TopicHzCmd(std::pmr::memory_resource* mr)
      : topic_name(mr), message_type(mr) {}
};

/// @brief Request topic bandwidth (bw) measurement.
struct TopicBwCmd {
  static constexpr std::string_view kName = "TopicBwCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  std::pmr::string message_type;
  uint32_t window = 10;
  bool wall_time = false;

  TopicBwCmd() : TopicBwCmd(std::pmr::get_default_resource()) {}
  explicit TopicBwCmd(std::pmr::memory_resource* mr)
      : topic_name(mr), message_type(mr) {}
};

/// @brief Response for `TopicInfoCmd`.
struct TopicInfoResponseCmd {
  static constexpr std::string_view kName = "TopicInfoResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  std::pmr::string message_type;
  uint32_t publisher_count = 0;
  uint32_t subscriber_count = 0;
  std::pmr::vector<std::pmr::string> publisher_nodes;
  std::pmr::vector<std::pmr::string> subscriber_nodes;

  TopicInfoResponseCmd()
      : TopicInfoResponseCmd(std::pmr::get_default_resource()) {}
  explicit TopicInfoResponseCmd(std::pmr::memory_resource* mr)
      : topic_name(mr),
        message_type(mr),
        publisher_nodes(mr),
        subscriber_nodes(mr) {}
};

/// @brief Push a topic message payload to a subscribed mod client.
struct TopicPayloadCmd {
  static constexpr std::string_view kName = "TopicPayloadCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  std::pmr::string message_type;

  std::pmr::vector<uint8_t> payload;  ///< Raw CDR bytes
  bool raw = false;

  TopicPayloadCmd() : TopicPayloadCmd(std::pmr::get_default_resource()) {}
  explicit TopicPayloadCmd(std::pmr::memory_resource* mr)
      : topic_name(mr), message_type(mr), payload(mr) {}
};

/// @brief Response for `TopicHzCmd` — topic frequency measurement.
struct TopicHzResponseCmd {
  static constexpr std::string_view kName = "TopicHzResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  double frequency = 0.0;
  uint32_t window = 0;
  uint32_t message_count = 0;

  TopicHzResponseCmd() : TopicHzResponseCmd(std::pmr::get_default_resource()) {}
  explicit TopicHzResponseCmd(std::pmr::memory_resource* mr) : topic_name(mr) {}
};

/// @brief Response for `TopicBwCmd` — topic bandwidth measurement.
struct TopicBwResponseCmd {
  static constexpr std::string_view kName = "TopicBwResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  double bytes_per_second = 0.0;
  uint32_t window = 0;
  uint32_t message_count = 0;
  uint64_t total_bytes = 0;

  TopicBwResponseCmd() : TopicBwResponseCmd(std::pmr::get_default_resource()) {}
  explicit TopicBwResponseCmd(std::pmr::memory_resource* mr) : topic_name(mr) {}
};

/// @brief Request topic delay measurement (header stamp vs wall clock).
struct TopicDelayCmd {
  static constexpr std::string_view kName = "TopicDelayCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  std::pmr::string message_type;
  uint32_t window = 10;

  TopicDelayCmd() : TopicDelayCmd(std::pmr::get_default_resource()) {}
  explicit TopicDelayCmd(std::pmr::memory_resource* mr)
      : topic_name(mr), message_type(mr) {}
};

/// @brief Response for `TopicDelayCmd` — topic delay measurement.
struct TopicDelayResponseCmd {
  static constexpr std::string_view kName = "TopicDelayResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  double average_delay = 0.0;
  double min_delay = 0.0;
  double max_delay = 0.0;
  uint32_t window = 0;
  uint32_t message_count = 0;

  TopicDelayResponseCmd()
      : TopicDelayResponseCmd(std::pmr::get_default_resource()) {}
  explicit TopicDelayResponseCmd(std::pmr::memory_resource* mr)
      : topic_name(mr) {}
};

}  // namespace roscraft::bridge
