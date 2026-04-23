#pragma once

#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>

namespace roscraft::bridge {

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
