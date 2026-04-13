#include <doctest/doctest.h>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/app/config.hpp>
#include <roscraft/bridge/bridge.hpp>
#include <roscraft/bridge/network/runner.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string_view>

using namespace roscraft::bridge;
using namespace roscraft::bridge::network;

namespace {

class DummyBridge final : public Bridge {
public:
  void Init(App&) override {
    if (on_init) {
      on_init();
    }
  }

  void Destroy(App&) override {
    if (on_destroy) {
      on_destroy();
    }
  }

  void Reload(App&) override {
    if (on_reload) {
      on_reload();
    }
  }

  void Tick(App&) override {
    if (on_tick) {
      on_tick();
    }
  }

  [[nodiscard]] BridgeStatus Status() const noexcept override {
    return BridgeStatus::kReady;
  }

  std::function<void()> on_init;
  std::function<void()> on_destroy;
  std::function<void()> on_reload;
  std::function<void()> on_tick;
};

class ScopedAppGuard {
public:
  ScopedAppGuard() { App::Instance().Shutdown(); }
  ScopedAppGuard(const ScopedAppGuard&) = delete;
  ScopedAppGuard(ScopedAppGuard&&) = delete;
  ~ScopedAppGuard() { App::Instance().Shutdown(); }

  ScopedAppGuard& operator=(const ScopedAppGuard&) = delete;
  ScopedAppGuard& operator=(ScopedAppGuard&&) = delete;
};

}  // namespace

TEST_SUITE("bridge::network::RunnerError") {
  TEST_CASE("bridge::network::RunnerError enum values") {
    CHECK_EQ(static_cast<uint8_t>(RunnerError::kAppNotInitialized),
             static_cast<uint8_t>(1));
    CHECK_EQ(static_cast<uint8_t>(RunnerError::kUnexpectedShutdown),
             static_cast<uint8_t>(2));
  }

  TEST_CASE("bridge::network::RunnerErrorToString") {
    CHECK_EQ(RunnerErrorToString(RunnerError::kAppNotInitialized),
             std::string_view{"App not initialized"});
    CHECK_EQ(RunnerErrorToString(RunnerError::kUnexpectedShutdown),
             std::string_view{"Unexpected shutdown"});

    constexpr auto unknown = static_cast<RunnerError>(255);
    CHECK_EQ(RunnerErrorToString(unknown), std::string_view{"Unknown error"});
  }

  TEST_CASE("bridge::network::RunnerErrorToExitCode") {
    CHECK_EQ(RunnerErrorToExitCode(RunnerError::kAppNotInitialized), 1);
    CHECK_EQ(RunnerErrorToExitCode(RunnerError::kUnexpectedShutdown), 2);

    constexpr auto unknown = static_cast<RunnerError>(255);
    CHECK_EQ(RunnerErrorToExitCode(unknown), 0);
  }

  TEST_CASE("bridge::network::Run") {
    SUBCASE("Returns kAppNotInitialized when app is not initialized") {
      ScopedAppGuard app_guard;
      auto& app = App::Instance();

      const auto result = Run(app);

      CHECK_FALSE(result.has_value());
      REQUIRE_FALSE(result.has_value());
      CHECK_EQ(result.error(), RunnerError::kAppNotInitialized);
    }

    SUBCASE("Returns success when shutdown is already requested") {
      ScopedAppGuard app_guard;
      auto& app = App::Instance();

      app.Init(AppConfig::From<DummyBridge>());
      app.RequestShutdown();

      const auto result = Run(app);

      CHECK(result.has_value());
      CHECK_EQ(app.State(), AppState::kUninitialized);
    }

    SUBCASE("Ticks at least once when shutdown is requested asynchronously") {
      ScopedAppGuard app_guard;
      auto& app = App::Instance();

      app.Init(AppConfig::From<DummyBridge>());

      auto& bridge = app.GetBridge<DummyBridge>();
      int tick_calls = 0;
      bridge.on_tick = [&] {
        ++tick_calls;
        app.RequestShutdown();
      };

      const auto result = Run(app);

      CHECK(result.has_value());
      CHECK_GE(tick_calls, 1);
      CHECK_EQ(app.State(), AppState::kUninitialized);
    }
  }
}
