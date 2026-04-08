#include <pch.hpp>

#include <roscraft/bridge/network/runner.hpp>

#include <roscraft/bridge/app/app.hpp>

#include <rclcpp/logging.hpp>

#include <chrono>
#include <csignal>
#include <expected>
#include <functional>
#include <optional>
#include <thread>

namespace roscraft::bridge::network {

namespace {

std::optional<std::reference_wrapper<App>> g_app_instance;

void SignalHandler(int /*signal*/) {
  if (g_app_instance.has_value()) {
    g_app_instance->get().RequestShutdown();
  }
}

void InstallSignalHandlers(App& app) noexcept {
  g_app_instance.emplace(app);
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);
}

void RestoreSignalHandlers() noexcept {
  std::signal(SIGINT, SIG_DFL);
  std::signal(SIGTERM, SIG_DFL);
  g_app_instance.reset();
}

}  // namespace

auto Run(App& app) -> std::expected<void, RunnerError> {
  if (app.State() != AppState::kInitialized) [[unlikely]] {
    constexpr auto error = RunnerError::kAppNotInitialized;
    RCLCPP_ERROR(rclcpp::get_logger("NetworkBridge"),
                 "Runner error: %s. Call App::Init() before Runner::Run().",
                 RunnerErrorToString(error).data());
    return std::unexpected(error);
  }

  InstallSignalHandlers(app);

  RCLCPP_INFO(rclcpp::get_logger("NetworkBridge"),
              "Network bridge running. Entering tick loop...");

  while (!app.IsShutdownRequested()) {
    app.Tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  RCLCPP_INFO(rclcpp::get_logger("NetworkBridge"),
              "Shutdown requested, cleaning up...");

  app.Shutdown();

  RestoreSignalHandlers();

  return {};
}

}  // namespace roscraft::bridge::network
