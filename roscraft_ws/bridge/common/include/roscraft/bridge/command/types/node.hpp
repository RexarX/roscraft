#pragma once

#include <roscraft/bridge/command/types/common.hpp>

#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

/// @brief Query detailed information about a specific topic.
struct NodeInfoCmd {
  static constexpr std::string_view kName = "NodeInfoCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  bool include_hidden = false;

  NodeInfoCmd() : NodeInfoCmd(std::pmr::get_default_resource()) {}
  explicit NodeInfoCmd(std::pmr::memory_resource* mr) : node_name(mr) {}
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

}  // namespace roscraft::bridge
