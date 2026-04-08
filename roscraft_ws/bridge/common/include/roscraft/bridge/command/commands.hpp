#pragma once

/// @file commands.hpp
/// @brief All command types flowing between the Minecraft mod and the ROS2
/// bridge layer.
///
/// Naming convention: every command type carries a "Cmd" suffix.
/// Incoming  = mod  → ROS  (requests).
/// Outgoing  = ROS  → mod  (responses / pushes).
///
/// PMR note: containers inside commands use std::pmr allocators so hot-path
/// callers can supply an arena and avoid heap traffic.  Commands that travel
/// through the CommandQueue use the default PMR resource (swappable globally).

#include <roscraft/bridge/command/command.hpp>

#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

// ============================================================================
// Incoming commands  (mod → ROS)
// ============================================================================

/// @brief Query all topics / services / actions visible in the ROS2 graph.
struct QueryGraphCmd {
  static constexpr std::string_view kName = "QueryGraphCmd";

  uint64_t request_id = 0;
};

/// @brief Ask the bridge to start forwarding a topic's messages to the mod.
struct SubscribeTopicCmd {
  static constexpr std::string_view kName = "SubscribeTopicCmd";

  uint64_t request_id = 0;
  std::pmr::string topic_name;
  std::pmr::string message_type;  ///< e.g. "geometry_msgs/msg/Twist"

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

/// @brief Request player positions from the mod.
struct QueryPlayersCmd {
  static constexpr std::string_view kName = "QueryPlayersCmd";

  uint64_t request_id = 0;
};

// ============================================================================
// Outgoing commands  (ROS → mod)
// ============================================================================

/// @brief Instruct the mod to place a block in the Minecraft world.
struct PlaceBlockCmd {
  static constexpr std::string_view kName = "PlaceBlockCmd";

  int32_t x = 0;
  int32_t y = 0;
  int32_t z = 0;
  std::pmr::string block_id;  ///< e.g. "minecraft:stone"

  PlaceBlockCmd() : PlaceBlockCmd(std::pmr::get_default_resource()) {}
  explicit PlaceBlockCmd(std::pmr::memory_resource* mr) : block_id(mr) {}
};

/// @brief Response to QueryGraphCmd — full snapshot of the ROS2 graph.
struct GraphSnapshotCmd {
  static constexpr std::string_view kName = "GraphSnapshotCmd";

  uint64_t request_id = 0;
  std::pmr::vector<std::pmr::string> topics;
  std::pmr::vector<std::pmr::string> services;
  std::pmr::vector<std::pmr::string> actions;

  GraphSnapshotCmd() : GraphSnapshotCmd(std::pmr::get_default_resource()) {}
  explicit GraphSnapshotCmd(std::pmr::memory_resource* mr)
      : topics(mr), services(mr), actions(mr) {}
};

/// @brief Push a topic message payload to a subscribed mod client.
struct TopicPayloadCmd {
  static constexpr std::string_view kName = "TopicPayloadCmd";

  std::pmr::string topic_name;
  std::pmr::string message_type;
  std::pmr::vector<uint8_t> payload;  ///< Raw CDR bytes

  TopicPayloadCmd() : TopicPayloadCmd(std::pmr::get_default_resource()) {}
  explicit TopicPayloadCmd(std::pmr::memory_resource* mr)
      : topic_name(mr), message_type(mr), payload(mr) {}
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

}  // namespace roscraft::bridge
