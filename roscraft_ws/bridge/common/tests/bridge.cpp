#include <doctest/doctest.h>

#include <roscraft/bridge/bridge.hpp>

#include <concepts>
#include <cstdint>
#include <string_view>

using namespace roscraft::bridge;

namespace {

class DummyBridge final : public Bridge {
public:
  void Init(App&) override { status_ = BridgeStatus::kInitializing; }
  void Destroy(App&) override { status_ = BridgeStatus::kShuttingDown; }
  void Reload(App&) override { status_ = BridgeStatus::kReady; }
  void Tick(App&) override { status_ = BridgeStatus::kReady; }

  [[nodiscard]] BridgeStatus Status() const noexcept override {
    return status_;
  }

private:
  BridgeStatus status_ = BridgeStatus::kUninitialized;
};

template <typename T>
concept BridgeInterfaceTrait = requires(T bridge, App& app) {
  { bridge.Init(app) } -> std::same_as<void>;
  { bridge.Destroy(app) } -> std::same_as<void>;
  { bridge.Reload(app) } -> std::same_as<void>;
  { bridge.Tick(app) } -> std::same_as<void>;
  { bridge.Status() } -> std::same_as<BridgeStatus>;
};

static_assert(BridgeTrait<DummyBridge>);
static_assert(!BridgeTrait<int>);
static_assert(BridgeInterfaceTrait<DummyBridge>);

}  // namespace

TEST_SUITE("bridge::BridgeStatus") {
  TEST_CASE("bridge::ToString") {
    SUBCASE("kUninitialized") {
      CHECK_EQ(ToString(BridgeStatus::kUninitialized), "Uninitialized");
    }

    SUBCASE("kInitializing") {
      CHECK_EQ(ToString(BridgeStatus::kInitializing), "Initializing");
    }

    SUBCASE("kReady") {
      CHECK_EQ(ToString(BridgeStatus::kReady), "Ready");
    }

    SUBCASE("kError") {
      CHECK_EQ(ToString(BridgeStatus::kError), "Error");
    }

    SUBCASE("kShuttingDown") {
      CHECK_EQ(ToString(BridgeStatus::kShuttingDown), "ShuttingDown");
    }

    SUBCASE("Unknown value") {
      constexpr auto kUnknown = static_cast<BridgeStatus>(UINT8_MAX);
      CHECK_EQ(ToString(kUnknown), "Unknown");
    }
  }
}

TEST_SUITE("bridge::Bridge") {
  TEST_CASE("bridge::BridgeTrait") {
    CHECK(BridgeTrait<DummyBridge>);
    CHECK_FALSE(BridgeTrait<int>);
  }

  TEST_CASE("bridge::Bridge interface") {
    CHECK(BridgeInterfaceTrait<DummyBridge>);

    DummyBridge bridge;
    CHECK_EQ(bridge.Status(), BridgeStatus::kUninitialized);
  }
}
