#include <pch.hpp>

#include <roscraft/bridge/assert.hpp>

#include <rclcpp/logging.hpp>

#include <source_location>
#include <string>
#include <string_view>

namespace roscraft::details {

bool HasLoggerHandler() noexcept {
  return true;
}

void LoggerAssertionHandler(std::string_view condition,
                            const std::source_location& loc,
                            std::string_view message) noexcept {
  if (message.empty()) {
    RCLCPP_FATAL(rclcpp::get_logger("roscraft_bridge"),
                 "Assertion failed: %s\n  File: %s:%u\n  Function: %s",
                 condition.data(), loc.file_name(), loc.line(),
                 loc.function_name());
  } else {
    RCLCPP_FATAL(rclcpp::get_logger("roscraft_bridge"),
                 "Assertion failed: %s | %s\n  File: %s:%u\n  Function: %s",
                 condition.data(), message.data(), loc.file_name(),
                 loc.line(), loc.function_name());
  }
}

}  // namespace roscraft::details
