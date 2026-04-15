#pragma once

#include <roscraft/bridge/command/command.hpp>

#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

// ============================================================================
// Incoming commands  (mod -> ROS)
// ============================================================================

/// @brief Query all topics / services / actions visible in the ROS2 graph.
struct QueryGraphCmd {
  static constexpr std::string_view kName = "QueryGraphCmd";

  uint64_t request_id = 0;
};

/// @brief Query detailed information about a specific topic.
struct NodeInfoCmd {
  static constexpr std::string_view kName = "NodeInfoCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  bool include_hidden = false;

  NodeInfoCmd() : NodeInfoCmd(std::pmr::get_default_resource()) {}
  explicit NodeInfoCmd(std::pmr::memory_resource* mr) : node_name(mr) {}
};

/// @brief Query detailed information about a specific topic.
struct TopicInfoCmd {
  static constexpr std::string_view kName = "TopicInfoCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;

  TopicInfoCmd() : TopicInfoCmd(std::pmr::get_default_resource()) {}
  explicit TopicInfoCmd(std::pmr::memory_resource* mr) : topic_name(mr) {}
};

/// @brief Query detailed information about a specific service.
struct ServiceInfoCmd {
  static constexpr std::string_view kName = "ServiceInfoCmd";

  uint64_t request_id = 0;
  std::pmr::string service_name;

  ServiceInfoCmd() : ServiceInfoCmd(std::pmr::get_default_resource()) {}
  explicit ServiceInfoCmd(std::pmr::memory_resource* mr) : service_name(mr) {}
};

/// @brief Query full interface definition text by interface type string.
struct InterfaceListCmd {
  static constexpr std::string_view kName = "InterfaceListCmd";

  uint64_t request_id = 0;
  bool include_messages = true;
  bool include_services = true;
  bool include_actions = true;
};

/// @brief Query full interface definition text by interface type string.
struct InterfaceShowCmd {
  static constexpr std::string_view kName = "InterfaceShowCmd";

  uint64_t request_id = 0;
  std::pmr::string interface_type;

  InterfaceShowCmd() : InterfaceShowCmd(std::pmr::get_default_resource()) {}
  explicit InterfaceShowCmd(std::pmr::memory_resource* mr)
      : interface_type(mr) {}
};

/// @brief Ask the bridge to start forwarding a topic's messages to the mod.
struct SubscribeTopicCmd {
  static constexpr std::string_view kName = "SubscribeTopicCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  std::pmr::string message_type;  ///< e.g. "geometry_msgs/msg/Twist"
  bool once = false;
  double timeout_seconds = 0.0;
  bool raw = false;

  SubscribeTopicCmd() : SubscribeTopicCmd(std::pmr::get_default_resource()) {}
  explicit SubscribeTopicCmd(std::pmr::memory_resource* mr)
      : topic_name(mr), message_type(mr) {}
};

/// @brief Publish a raw CDR-serialized message onto a ROS topic.
struct PublishMessageCmd {
  static constexpr std::string_view kName = "PublishMessageCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  std::pmr::string message_type;
  std::pmr::vector<uint8_t> payload;  ///< Raw CDR bytes

  PublishMessageCmd() : PublishMessageCmd(std::pmr::get_default_resource()) {}
  explicit PublishMessageCmd(std::pmr::memory_resource* mr)
      : topic_name(mr), message_type(mr), payload(mr) {}
};

/// @brief Request topic frequency (hz) measurement.
struct TopicHzCmd {
  static constexpr std::string_view kName = "TopicHzCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  std::pmr::string message_type;
  uint32_t window = 10;

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

  TopicBwCmd() : TopicBwCmd(std::pmr::get_default_resource()) {}
  explicit TopicBwCmd(std::pmr::memory_resource* mr)
      : topic_name(mr), message_type(mr) {}
};

/// @brief Request player positions from the mod.
struct QueryPlayersCmd {
  static constexpr std::string_view kName = "QueryPlayersCmd";

  uint64_t request_id = 0;
};

// ============================================================================
// Outgoing commands  (ROS -> mod)
// ============================================================================

/// @brief A topic name and its associated message type.
struct TopicEntry {
  std::pmr::string name;
  std::pmr::string type;

  TopicEntry() : TopicEntry(std::pmr::get_default_resource()) {}
  explicit TopicEntry(std::pmr::memory_resource* mr) : name(mr), type(mr) {}
};

/// @brief A service name and its associated service type.
struct ServiceEntry {
  std::pmr::string name;
  std::pmr::string type;

  ServiceEntry() : ServiceEntry(std::pmr::get_default_resource()) {}
  explicit ServiceEntry(std::pmr::memory_resource* mr) : name(mr), type(mr) {}
};

/// @brief An action name and its associated action type.
struct ActionEntry {
  std::pmr::string name;
  std::pmr::string type;

  ActionEntry() : ActionEntry(std::pmr::get_default_resource()) {}
  explicit ActionEntry(std::pmr::memory_resource* mr) : name(mr), type(mr) {}
};

/// @brief A ROS2 node name.
struct NodeEntry {
  std::pmr::string name;

  NodeEntry() : NodeEntry(std::pmr::get_default_resource()) {}
  explicit NodeEntry(std::pmr::memory_resource* mr) : name(mr) {}
};

/// @brief Response to QueryGraphCmd — full snapshot of the ROS2 graph.
struct GraphSnapshotCmd {
  static constexpr std::string_view kName = "GraphSnapshotCmd";

  uint64_t request_id = 0;
  std::pmr::vector<NodeEntry> nodes;
  std::pmr::vector<TopicEntry> topics;
  std::pmr::vector<ServiceEntry> services;
  std::pmr::vector<ActionEntry> actions;

  GraphSnapshotCmd() : GraphSnapshotCmd(std::pmr::get_default_resource()) {}
  explicit GraphSnapshotCmd(std::pmr::memory_resource* mr)
      : nodes(mr), topics(mr), services(mr), actions(mr) {}
};

/// @brief Response for `TopicInfoCmd`.
struct NodeInfoResponseCmd {
  static constexpr std::string_view kName = "NodeInfoResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  std::pmr::vector<TopicEntry> publishers;
  std::pmr::vector<TopicEntry> subscribers;
  std::pmr::vector<ServiceEntry> services;
  bool found = false;

  NodeInfoResponseCmd()
      : NodeInfoResponseCmd(std::pmr::get_default_resource()) {}
  explicit NodeInfoResponseCmd(std::pmr::memory_resource* mr)
      : node_name(mr), publishers(mr), subscribers(mr), services(mr) {}
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

/// @brief Response for `ServiceInfoCmd`.
struct ServiceInfoResponseCmd {
  static constexpr std::string_view kName = "ServiceInfoResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string service_name;
  std::pmr::string service_type;
  uint32_t client_count = 0;
  uint32_t server_count = 0;
  std::pmr::vector<std::pmr::string> client_nodes;
  std::pmr::vector<std::pmr::string> server_nodes;

  ServiceInfoResponseCmd()
      : ServiceInfoResponseCmd(std::pmr::get_default_resource()) {}
  explicit ServiceInfoResponseCmd(std::pmr::memory_resource* mr)
      : service_name(mr),
        service_type(mr),
        client_nodes(mr),
        server_nodes(mr) {}
};

/// @brief Response for `InterfaceShowCmd`.
struct InterfaceListResponseCmd {
  static constexpr std::string_view kName = "InterfaceListResponseCmd";

  uint64_t request_id = 0;
  std::pmr::vector<std::pmr::string> messages;
  std::pmr::vector<std::pmr::string> services;
  std::pmr::vector<std::pmr::string> actions;

  InterfaceListResponseCmd()
      : InterfaceListResponseCmd(std::pmr::get_default_resource()) {}
  explicit InterfaceListResponseCmd(std::pmr::memory_resource* mr)
      : messages(mr), services(mr), actions(mr) {}
};

/// @brief Response for `InterfaceShowCmd`.
struct InterfaceShowResponseCmd {
  static constexpr std::string_view kName = "InterfaceShowResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string interface_type;
  std::pmr::string definition;
  bool found = false;

  InterfaceShowResponseCmd()
      : InterfaceShowResponseCmd(std::pmr::get_default_resource()) {}
  explicit InterfaceShowResponseCmd(std::pmr::memory_resource* mr)
      : interface_type(mr), definition(mr) {}
};

/// @brief Push a topic message payload to a subscribed mod client.
struct TopicPayloadCmd {
  static constexpr std::string_view kName = "TopicPayloadCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  std::pmr::string message_type;
  bool raw = false;
  std::pmr::vector<uint8_t> payload;  ///< Raw CDR bytes

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

struct Player {
  std::pmr::string name;
  float x = 0.F;
  float y = 0.F;
  float z = 0.F;

  Player() : Player(std::pmr::get_default_resource()) {}
  explicit Player(std::pmr::memory_resource* mr) : name(mr) {}
};

/// @brief Response to QueryPlayersCmd.
struct PlayerListCmd {
  static constexpr std::string_view kName = "PlayerListCmd";

  uint64_t request_id = 0;
  std::pmr::vector<Player> players;

  PlayerListCmd() : PlayerListCmd(std::pmr::get_default_resource()) {}
  explicit PlayerListCmd(std::pmr::memory_resource* mr) : players(mr) {}
};

/// @brief Error response sent when an incoming command fails.
/// @details Carries the request_id from the originating command so the mod
/// can correlate the error with the user request.
struct ErrorCmd {
  static constexpr std::string_view kName = "ErrorCmd";

  uint64_t request_id = 0;
  std::pmr::string error_code;
  std::pmr::string error_message;

  ErrorCmd() : ErrorCmd(std::pmr::get_default_resource()) {}
  explicit ErrorCmd(std::pmr::memory_resource* mr)
      : error_code(mr), error_message(mr) {}
};

}  // namespace roscraft::bridge
