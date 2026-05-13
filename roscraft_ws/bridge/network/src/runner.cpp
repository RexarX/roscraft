#include <pch.hpp>

#include <roscraft/bridge/network/runner.hpp>

#include <roscraft/bridge/app/app.hpp>

#include <rclcpp/logging.hpp>

#include <chrono>
#include <csignal>
#include <expected>
#include <functional>
#include <thread>

namespace roscraft::bridge::network {

namespace {

void SignalHandler(int /*signal*/) {
  auto& app = App::Instance();
  app.RequestShutdown();
}

void InstallSignalHandlers() noexcept {
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);
}

void RestoreSignalHandlers() noexcept {
  std::signal(SIGINT, SIG_DFL);
  std::signal(SIGTERM, SIG_DFL);
}

}  // namespace

auto Run(App& app) -> std::expected<void, RunnerError> {
  if (app.State() != AppState::kInitialized) [[unlikely]] {
    constexpr auto error = RunnerError::kAppNotInitialized;
    RCLCPP_ERROR(rclcpp::get_logger("NetworkBridge"),
                 "Runner error: %s. Call App::Init() before Runner::Run()!",
                 RunnerErrorToString(error).data());
    return std::unexpected(error);
  }

  InstallSignalHandlers();

  RCLCPP_INFO(rclcpp::get_logger("NetworkBridge"),
              "Network bridge running\nEntering tick loop...");

  while (!app.IsShutdownRequested()) {
    app.Tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  app.Shutdown();

  RestoreSignalHandlers();

  return {};
}

}  // namespace roscraft::bridge::network
