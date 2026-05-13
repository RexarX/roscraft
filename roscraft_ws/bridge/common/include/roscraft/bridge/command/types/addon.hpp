#pragma once

#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

/// @brief Bidirectional addon event packet.
struct AddonEventCmd {
  static constexpr std::string_view kName = "AddonEventCmd";

  uint64_t request_id = 0;
  std::pmr::string addon_id;
  std::pmr::string event_type;
  std::pmr::string encoding;
  std::pmr::vector<uint8_t> payload;
  bool response = false;

  AddonEventCmd() : AddonEventCmd(std::pmr::get_default_resource()) {}
  explicit AddonEventCmd(std::pmr::memory_resource* mr)
      : addon_id(mr), event_type(mr), encoding(mr), payload(mr) {}
};

}  // namespace roscraft::bridge
