#include <doctest/doctest.h>

#include <roscraft/bridge/app/app.hpp>

#include <roscraft/bridge/command/commands.hpp>

#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <thread>
#include <type_traits>

using namespace roscraft::bridge;

namespace {

class DummyBridge final : public Bridge {
public:
  DummyBridge() = default;
  explicit DummyBridge(int marker) : marker_(marker) {}
  ~DummyBridge() override { ++destructor_count; }

  void Init(App&) override {
    ++init_count;
    status_ = BridgeStatus::kInitializing;
  }

  void Destroy(App&) override {
    ++destroy_count;
    status_ = BridgeStatus::kShuttingDown;
  }

  void Reload(App&) override {
    ++reload_count;
    status_ = BridgeStatus::kReady;
  }

  void Tick(App&) override {
    ++tick_count;
    status_ = BridgeStatus::kReady;
  }

  [[nodiscard]] BridgeStatus Status() const noexcept override {
    return status_;
  }

  [[nodiscard]] int Marker() const noexcept { return marker_; }

  static void ResetCounters() {
    init_count = 0;
    destroy_count = 0;
    reload_count = 0;
    tick_count = 0;
    destructor_count = 0;
  }

  static inline int init_count = 0;
  static inline int destroy_count = 0;
  static inline int reload_count = 0;
  static inline int tick_count = 0;
  static inline int destructor_count = 0;

private:
  int marker_ = 0;
  BridgeStatus status_ = BridgeStatus::kUninitialized;
};

class ScopedAppGuard {
public:
  ScopedAppGuard() {
    auto& app = App::Instance();
    app.Shutdown();
    DummyBridge::ResetCounters();
  }

  ScopedAppGuard(const ScopedAppGuard&) = delete;
  ScopedAppGuard(ScopedAppGuard&&) = delete;
  ~ScopedAppGuard() { App::Instance().Shutdown(); }

  ScopedAppGuard& operator=(const ScopedAppGuard&) = delete;
  ScopedAppGuard& operator=(ScopedAppGuard&&) = delete;
};

[[nodiscard]] AppConfig MakeConfig(int marker = 0) {
  return AppConfig::From<DummyBridge>(marker);
}

}  // namespace

TEST_SUITE("bridge::App") {
  TEST_CASE("bridge::App::Instance") {
    App& first = App::Instance();
    App& second = App::Instance();

    CHECK_EQ(&first, &second);
  }

  TEST_CASE("bridge::App::Init") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig(7));

    CHECK_EQ(app.State(), AppState::kInitialized);
    CHECK_EQ(DummyBridge::init_count, 1);
    CHECK_EQ(app.GetBridge<DummyBridge>().Marker(), 7);

    const auto& inc = app.IncomingQueue();
    CHECK(inc.IsRegistered<ActionInfoCmd>());
    CHECK(inc.IsRegistered<ActionSendGoalCmd>());
    CHECK(inc.IsRegistered<QueryGraphCmd>());
    CHECK(inc.IsRegistered<InterfaceListCmd>());
    CHECK(inc.IsRegistered<InterfaceShowCmd>());
    CHECK(inc.IsRegistered<NodeInfoCmd>());
    CHECK(inc.IsRegistered<ParamDescribeCmd>());
    CHECK(inc.IsRegistered<ParamDumpCmd>());
    CHECK(inc.IsRegistered<ParamGetCmd>());
    CHECK(inc.IsRegistered<TopicBwCmd>());
    CHECK(inc.IsRegistered<ParamListCmd>());
    CHECK(inc.IsRegistered<ParamLoadCmd>());
    CHECK(inc.IsRegistered<ParamSetCmd>());
    CHECK(inc.IsRegistered<QueryPlayersCmd>());
    CHECK(inc.IsRegistered<ServiceCallCmd>());
    CHECK(inc.IsRegistered<ServiceInfoCmd>());
    CHECK(inc.IsRegistered<TopicSubscribeCmd>());
    CHECK(inc.IsRegistered<TopicPublishMessageCmd>());
    CHECK(inc.IsRegistered<TopicHzCmd>());
    CHECK(inc.IsRegistered<TopicBwCmd>());
    CHECK(inc.IsRegistered<TopicDelayCmd>());

    const auto& out = app.OutgoingQueue();
    CHECK(out.IsRegistered<ActionInfoResponseCmd>());
    CHECK(out.IsRegistered<ActionFeedbackCmd>());
    CHECK(out.IsRegistered<ActionResultCmd>());
    CHECK(out.IsRegistered<ErrorCmd>());
    CHECK(out.IsRegistered<GraphSnapshotCmd>());
    CHECK(out.IsRegistered<InterfaceListResponseCmd>());
    CHECK(out.IsRegistered<InterfaceShowResponseCmd>());
    CHECK(out.IsRegistered<NodeInfoResponseCmd>());
    CHECK(out.IsRegistered<ParamDescribeResponseCmd>());
    CHECK(out.IsRegistered<ParamDumpResponseCmd>());
    CHECK(out.IsRegistered<ParamGetResponseCmd>());
    CHECK(out.IsRegistered<ParamLoadResponseCmd>());
    CHECK(out.IsRegistered<ParamListResponseCmd>());
    CHECK(out.IsRegistered<ParamSetResponseCmd>());
    CHECK(out.IsRegistered<PlayerListCmd>());
    CHECK(out.IsRegistered<ServiceCallResponseCmd>());
    CHECK(out.IsRegistered<ServiceInfoResponseCmd>());
    CHECK(out.IsRegistered<TopicInfoResponseCmd>());
    CHECK(out.IsRegistered<TopicPayloadCmd>());
    CHECK(out.IsRegistered<TopicHzResponseCmd>());
    CHECK(out.IsRegistered<TopicBwResponseCmd>());
    CHECK(out.IsRegistered<TopicDelayResponseCmd>());
  }

  TEST_CASE("bridge::App::Shutdown") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig());
    app.Shutdown();

    CHECK_EQ(app.State(), AppState::kUninitialized);
    CHECK_EQ(DummyBridge::destroy_count, 1);
    CHECK_EQ(app.CurrentFrameIndex(), 0);
    CHECK_EQ(app.IncomingQueue().CommandCount(), 0);
    CHECK_EQ(app.OutgoingQueue().CommandCount(), 0);
  }

  TEST_CASE("bridge::App::Tick") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig());

    const size_t first_index = app.CurrentFrameIndex();
    app.Tick();
    const size_t second_index = app.CurrentFrameIndex();

    CHECK_EQ(DummyBridge::tick_count, 1);
    CHECK_EQ(second_index,
             (first_index + 1) %
                 roscraft::memory::DoubleFrameAllocator::FrameCount());

    app.Tick();
    CHECK_EQ(DummyBridge::tick_count, 2);
  }

  TEST_CASE("bridge::App::RequestShutdown") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.RequestShutdown();

    CHECK(app.IsShutdownRequested());
  }

  TEST_CASE("bridge::App::WaitForShutdown") {
    using namespace std::chrono_literals;

    ScopedAppGuard guard;
    auto& app = App::Instance();

    std::thread requester([&app] {
      std::this_thread::sleep_for(10ms);
      app.RequestShutdown();
    });

    const auto begin = std::chrono::steady_clock::now();
    app.WaitForShutdown();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - begin)
                             .count();

    requester.join();

    CHECK(app.IsShutdownRequested());
    CHECK_LT(elapsed, 500);
  }

  TEST_CASE("bridge::App::AddNode(NodeBaseInterface::SharedPtr)") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig());

    const auto node = std::make_shared<rclcpp::Node>("app_add_node_base");
    app.AddNode(node->get_node_base_interface());

    CHECK_EQ(app.State(), AppState::kInitialized);
  }

  TEST_CASE("bridge::App::AddNode(const std::shared_ptr<rclcpp::Node>&)") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig());

    const auto node = std::make_shared<rclcpp::Node>("app_add_node_full");
    app.AddNode(node);

    CHECK_EQ(app.State(), AppState::kInitialized);
  }

  TEST_CASE("bridge::App::AdvanceFrame") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    const size_t before = app.CurrentFrameIndex();
    app.AdvanceFrame();

    CHECK_EQ(
        app.CurrentFrameIndex(),
        (before + 1) % roscraft::memory::DoubleFrameAllocator::FrameCount());
  }

  TEST_CASE("bridge::App::ResetAllocator") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    [[maybe_unused]] void* current = app.CurrentFrameAllocator().allocate(64U);
    [[maybe_unused]] void* pending = app.PendingFrameAllocator().allocate(64U);

    CHECK_FALSE(app.CurrentFrameArena().Empty());
    CHECK_FALSE(app.PendingFrameArena().Empty());

    app.ResetAllocator();

    CHECK(app.CurrentFrameArena().Empty());
    CHECK(app.PendingFrameArena().Empty());
  }

  TEST_CASE("bridge::App::IsShutdownRequested") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.RequestShutdown();

    CHECK(app.IsShutdownRequested());
  }

  TEST_CASE("bridge::App::State") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    CHECK_EQ(app.State(), AppState::kUninitialized);

    app.Init(MakeConfig());
    CHECK_EQ(app.State(), AppState::kInitialized);

    app.Shutdown();
    CHECK_EQ(app.State(), AppState::kUninitialized);
  }

  TEST_CASE("bridge::App::CurrentFrameIndex") {
    ScopedAppGuard guard;
    const auto& app = App::Instance();

    CHECK_LT(app.CurrentFrameIndex(),
             roscraft::memory::DoubleFrameAllocator::FrameCount());
  }

  TEST_CASE("bridge::App::PendingFrameIndex") {
    ScopedAppGuard guard;
    const auto& app = App::Instance();

    CHECK_EQ(app.PendingFrameIndex(),
             (app.CurrentFrameIndex() + 1) %
                 roscraft::memory::DoubleFrameAllocator::FrameCount());
  }

  TEST_CASE("bridge::App::CurrentFrameArena") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    auto& arena = app.CurrentFrameArena();
    CHECK_EQ(&arena, &app.Allocator().Arena(app.CurrentFrameIndex()));
  }

  TEST_CASE("bridge::App::CurrentFrameArena const") {
    ScopedAppGuard guard;
    const auto& app = App::Instance();

    const auto& arena = app.CurrentFrameArena();
    CHECK_EQ(&arena, &app.Allocator().Arena(app.CurrentFrameIndex()));
  }

  TEST_CASE("bridge::App::PendingFrameArena") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    auto& arena = app.PendingFrameArena();
    CHECK_EQ(&arena, &app.Allocator().Arena(app.PendingFrameIndex()));
  }

  TEST_CASE("bridge::App::PendingFrameArena const") {
    ScopedAppGuard guard;
    const auto& app = App::Instance();

    const auto& arena = app.PendingFrameArena();
    CHECK_EQ(&arena, &app.Allocator().Arena(app.PendingFrameIndex()));
  }

  TEST_CASE("bridge::App::CurrentFrameAllocator") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    auto& allocator = app.CurrentFrameAllocator();
    auto& arena = app.CurrentFrameArena();

    CHECK_EQ(&allocator, static_cast<std::pmr::memory_resource*>(&arena));
  }

  TEST_CASE("bridge::App::CurrentFrameAllocator const") {
    ScopedAppGuard guard;
    const auto& app = App::Instance();

    const auto& allocator = app.CurrentFrameAllocator();
    const auto& arena = app.CurrentFrameArena();

    CHECK_EQ(&allocator, static_cast<const std::pmr::memory_resource*>(&arena));
  }

  TEST_CASE("bridge::App::PendingFrameAllocator") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    auto& allocator = app.PendingFrameAllocator();
    auto& arena = app.PendingFrameArena();

    CHECK_EQ(&allocator, static_cast<std::pmr::memory_resource*>(&arena));
  }

  TEST_CASE("bridge::App::PendingFrameAllocator const") {
    ScopedAppGuard guard;
    const auto& app = App::Instance();

    const auto& allocator = app.PendingFrameAllocator();
    const auto& arena = app.PendingFrameArena();

    CHECK_EQ(&allocator, static_cast<const std::pmr::memory_resource*>(&arena));
  }

  TEST_CASE("bridge::App::GetBridge") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig(42));

    Bridge& bridge = app.GetBridge();
    auto& typed = app.GetBridge<DummyBridge>();

    CHECK_EQ(&bridge, static_cast<Bridge*>(&typed));
    CHECK_EQ(typed.Marker(), 42);
  }

  TEST_CASE("bridge::App::GetBridge const") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig(9));

    const auto& const_app = app;
    const Bridge& bridge = const_app.GetBridge();
    const auto& typed = const_app.GetBridge<DummyBridge>();

    CHECK_EQ(&bridge, static_cast<const Bridge*>(&typed));
    CHECK_EQ(typed.Marker(), 9);
  }

  TEST_CASE("bridge::App::GetBridge<T>") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig(123));

    auto& bridge = app.GetBridge<DummyBridge>();
    CHECK_EQ(bridge.Marker(), 123);
  }

  TEST_CASE("bridge::App::GetBridge<T> const") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig(321));

    const auto& const_app = app;
    const auto& bridge = const_app.GetBridge<DummyBridge>();
    CHECK_EQ(bridge.Marker(), 321);
  }

  TEST_CASE("bridge::App::IncomingQueue") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig());

    auto& queue = app.IncomingQueue();
    CHECK(queue.IsRegistered<QueryGraphCmd>());
  }

  TEST_CASE("bridge::App::IncomingQueue const") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig());

    const auto& queue = static_cast<const App&>(app).IncomingQueue();
    CHECK(queue.IsRegistered<QueryGraphCmd>());
  }

  TEST_CASE("bridge::App::OutgoingQueue") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig());

    auto& queue = app.OutgoingQueue();
    CHECK(queue.IsRegistered<GraphSnapshotCmd>());
  }

  TEST_CASE("bridge::App::OutgoingQueue const") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    app.Init(MakeConfig());

    const auto& queue = static_cast<const App&>(app).OutgoingQueue();
    CHECK(queue.IsRegistered<GraphSnapshotCmd>());
  }

  TEST_CASE("bridge::App::Allocator") {
    ScopedAppGuard guard;
    auto& app = App::Instance();

    auto& allocator = app.Allocator();
    CHECK_EQ(&allocator, &app.Allocator());
  }

  TEST_CASE("bridge::App::Allocator const") {
    ScopedAppGuard guard;
    const auto& app = App::Instance();

    const auto& allocator = app.Allocator();
    CHECK_EQ(&allocator, &app.Allocator());
  }
}
