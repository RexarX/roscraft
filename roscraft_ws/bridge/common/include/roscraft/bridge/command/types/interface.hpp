#pragma once

#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

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

}  // namespace roscraft::bridge
