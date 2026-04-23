#pragma once

#include <roscraft/bridge/command/types/common.hpp>

#include <cstdint>
#include <memory_resource>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

/// @brief Query all topics / services / actions visible in the ROS2 graph.
struct QueryGraphCmd {
  static constexpr std::string_view kName = "QueryGraphCmd";

  uint64_t request_id = 0;
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

}  // namespace roscraft::bridge
