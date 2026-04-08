#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

namespace roscraft::bridge {

class App;

namespace network {

/// @brief Runner error codes for the network runner.
enum class RunnerError : uint8_t {
  kAppNotInitialized = 1,
  kUnexpectedShutdown = 2,
};

/// @brief Converts a `RunnerError` to a string view.
/// @param error The error to convert
/// @return A string view representing the error
[[nodiscard]] constexpr std::string_view RunnerErrorToString(
    RunnerError error) noexcept {
  switch (error) {
    case RunnerError::kAppNotInitialized:
      return "App not initialized";
    case RunnerError::kUnexpectedShutdown:
      return "Unexpected shutdown";
    default:
      return "Unknown error";
  }
}

/// @brief Converts a `RunnerError` to an exit code.
/// @param error The error to convert
/// @return The exit code corresponding to the error
[[nodiscard]] constexpr int RunnerErrorToExitCode(RunnerError error) noexcept {
  switch (error) {
    case RunnerError::kAppNotInitialized:
      return 1;
    case RunnerError::kUnexpectedShutdown:
      return 2;
    default:
      return 0;
  }
}

/// @brief Runs the network bridge application.
/// @details The Runner manages the application lifecycle including signal
/// handling. It waits for shutdown requests via the App's event system.
[[nodiscard]] auto Run(App& app) -> std::expected<void, RunnerError>;

}  // namespace network

}  // namespace roscraft::bridge
