#include <doctest/doctest.h>

#include <roscraft/bridge/app/config.hpp>

#include <cstdint>
#include <memory>

using namespace roscraft::bridge;

namespace {

class DummyBridge final : public Bridge {
public:
  DummyBridge() = default;
  explicit DummyBridge(int marker) : marker_(marker) {}

  void Init(App&) override {}
  void Destroy(App&) override {}
  void Reload(App&) override {}
  void Tick(App&) override {}

  [[nodiscard]] BridgeStatus Status() const noexcept override {
    return BridgeStatus::kReady;
  }

  [[nodiscard]] int Marker() const noexcept { return marker_; }

private:
  int marker_ = 0;
};

}  // namespace

TEST_SUITE("bridge::AppConfig") {
  TEST_CASE("bridge::AppConfig::Argc") {
    SUBCASE("Default config has argc == 0") {
      constexpr AppConfig config;
      CHECK_EQ(config.Argc(), 0);
    }

    SUBCASE("From(argc, argv) stores argc") {
      char arg0[] = "app";
      char* argv[] = {arg0};

      const AppConfig config = AppConfig::From(1, argv);
      CHECK_EQ(config.Argc(), 1);
    }
  }

  TEST_CASE("bridge::AppConfig::Argv") {
    SUBCASE("Default config has null argv") {
      constexpr AppConfig config;
      CHECK_EQ(config.Argv(), nullptr);
    }

    SUBCASE("From(argc, argv) stores argv") {
      char arg0[] = "app";
      char arg1[] = "--flag";
      char* argv[] = {arg0, arg1};

      const AppConfig config = AppConfig::From(2, argv);
      CHECK_EQ(config.Argv(), argv);
    }
  }

  TEST_CASE("bridge::AppConfig::From(std::unique_ptr<Bridge>)") {
    auto bridge = std::make_unique<DummyBridge>();
    DummyBridge* const raw = bridge.get();

    AppConfig config = AppConfig::From(std::move(bridge));

    CHECK_EQ(bridge.get(), nullptr);
    CHECK_EQ(config.bridge.get(), raw);
    CHECK(config.Valid());
    CHECK_EQ(config.Argc(), 0);
    CHECK_EQ(config.Argv(), nullptr);
  }

  TEST_CASE("bridge::AppConfig::From<T>(Args&&...)") {
    AppConfig config = AppConfig::From<DummyBridge>(42);

    CHECK(config.Valid());

    auto* typed_bridge = static_cast<DummyBridge*>(config.bridge.get());
    CHECK_NE(typed_bridge, nullptr);
    CHECK_EQ(typed_bridge->Marker(), 42);
  }

  TEST_CASE("bridge::AppConfig::From(int, char*[])") {
    char arg0[] = "app";
    char arg1[] = "--port";
    char* argv[] = {arg0, arg1};

    const AppConfig config = AppConfig::From(2, argv);

    CHECK_EQ(config.Argc(), 2);
    CHECK_EQ(config.Argv(), argv);
    CHECK_FALSE(config.Valid());
    CHECK(config.HasCommandLineArgs());
  }

  TEST_CASE("bridge::AppConfig::From(std::unique_ptr<Bridge>, int, char*[])") {
    char arg0[] = "app";
    char* argv[] = {arg0};

    AppConfig config =
        AppConfig::From(std::make_unique<DummyBridge>(), 1, argv);

    CHECK(config.Valid());
    CHECK_EQ(config.Argc(), 1);
    CHECK_EQ(config.Argv(), argv);
    CHECK(config.HasCommandLineArgs());
  }

  TEST_CASE("bridge::AppConfig::From<T>(int, char*[], Args&&...)") {
    char arg0[] = "app";
    char arg1[] = "--seed=7";
    char* argv[] = {arg0, arg1};

    AppConfig config = AppConfig::From<DummyBridge>(2, argv, 7);

    CHECK(config.Valid());
    CHECK_EQ(config.Argc(), 2);
    CHECK_EQ(config.Argv(), argv);

    auto* typed_bridge = static_cast<DummyBridge*>(config.bridge.get());
    CHECK_NE(typed_bridge, nullptr);
    CHECK_EQ(typed_bridge->Marker(), 7);
  }

  TEST_CASE("bridge::AppConfig::Valid") {
    SUBCASE("Returns false for default config") {
      constexpr AppConfig config;
      CHECK_FALSE(config.Valid());
    }

    SUBCASE("Returns true when bridge is set") {
      const AppConfig config = AppConfig::From(std::make_unique<DummyBridge>());
      CHECK(config.Valid());
    }
  }

  TEST_CASE("bridge::AppConfig::HasCommandLineArgs") {
    SUBCASE("Returns false when argc is zero") {
      char arg0[] = "app";
      char* argv[] = {arg0};
      const AppConfig config = AppConfig::From(0, argv);
      CHECK_FALSE(config.HasCommandLineArgs());
    }

    SUBCASE("Returns false when argv is null") {
      const AppConfig config = AppConfig::From(1, nullptr);
      CHECK_FALSE(config.HasCommandLineArgs());
    }

    SUBCASE("Returns true when argc > 0 and argv is not null") {
      char arg0[] = "app";
      char* argv[] = {arg0};
      const AppConfig config = AppConfig::From(1, argv);
      CHECK(config.HasCommandLineArgs());
    }
  }
}
