#pragma once

#include <memory_resource>
#include <string>

namespace roscraft::bridge {

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

struct Player {
  std::pmr::string name;
  float x = 0.F;
  float y = 0.F;
  float z = 0.F;

  Player() : Player(std::pmr::get_default_resource()) {}
  explicit Player(std::pmr::memory_resource* mr) : name(mr) {}
};

}  // namespace roscraft::bridge
