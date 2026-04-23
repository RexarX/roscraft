#pragma once

#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

/// @brief Query detailed information about a specific service.
struct ServiceInfoCmd {
  static constexpr std::string_view kName = "ServiceInfoCmd";

  uint64_t request_id = 0;
  std::pmr::string service_name;

  ServiceInfoCmd() : ServiceInfoCmd(std::pmr::get_default_resource()) {}
  explicit ServiceInfoCmd(std::pmr::memory_resource* mr) : service_name(mr) {}
};

/// @brief Request a service call.
struct ServiceCallCmd {
  static constexpr std::string_view kName = "ServiceCallCmd";

  uint64_t request_id = 0;
  std::pmr::string service_name;
  std::pmr::string service_type;
  std::pmr::vector<uint8_t> payload;
  double timeout_seconds = 0.0;
  uint32_t repeat_count = 0;
  double rate_hz = 0.0;

  ServiceCallCmd() : ServiceCallCmd(std::pmr::get_default_resource()) {}
  explicit ServiceCallCmd(std::pmr::memory_resource* mr)
      : service_name(mr), service_type(mr), payload(mr) {}
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

/// @brief Response for `ServiceCallCmd`.
struct ServiceCallResponseCmd {
  static constexpr std::string_view kName = "ServiceCallResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string service_name;
  std::pmr::string service_type;

  std::pmr::vector<uint8_t> response_payload;
  std::pmr::string result_text;

  bool success = false;

  ServiceCallResponseCmd()
      : ServiceCallResponseCmd(std::pmr::get_default_resource()) {}
  explicit ServiceCallResponseCmd(std::pmr::memory_resource* mr)
      : service_name(mr),
        service_type(mr),
        response_payload(mr),
        result_text(mr) {}
};

}  // namespace roscraft::bridge
