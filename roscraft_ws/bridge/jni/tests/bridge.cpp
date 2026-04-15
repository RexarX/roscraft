#include <doctest/doctest.h>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/app/config.hpp>
#include <roscraft/bridge/bridge.hpp>
#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/jni/bridge.hpp>
#include <roscraft/generated/bridge_packets_generated.hpp>

#include <flatbuffers/flatbuffers.h>

#include <array>
#include <cstdint>
#include <memory_resource>
#include <optional>
#include <span>
#include <vector>

#include "support/fake_jni.hpp"

using namespace roscraft::bridge;
using namespace roscraft::bridge::jni;

namespace {

class ScopedAppGuard {
public:
  ScopedAppGuard() { App::Instance().Shutdown(); }
  ScopedAppGuard(const ScopedAppGuard&) = delete;
  ScopedAppGuard(ScopedAppGuard&&) = delete;
  ~ScopedAppGuard() { App::Instance().Shutdown(); }

  ScopedAppGuard& operator=(const ScopedAppGuard&) = delete;
  ScopedAppGuard& operator=(ScopedAppGuard&&) = delete;
};

[[nodiscard]] BridgeConfig MakeBridgeConfig(tests::FakeJavaVM& fake_vm) {
  return BridgeConfig{.jvm = fake_vm.Vm()};
}

flatbuffers::FlatBufferBuilder BuildQueryGraphPacket(uint64_t request_id) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto inner = fbs::CreateQueryGraphPacket(fbb, request_id);
  const auto root = fbs::CreateBridgePacket(
      fbb, fbs::PacketPayload::QueryGraphPacket, inner.Union());
  fbs::FinishBridgePacketBuffer(fbb, root);
  return fbb;
}

flatbuffers::FlatBufferBuilder BuildGraphSnapshotPacket(uint64_t request_id) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto inner = fbs::CreateGraphSnapshotPacket(fbb, request_id);
  const auto root = fbs::CreateBridgePacket(
      fbb, fbs::PacketPayload::GraphSnapshotPacket, inner.Union());
  fbs::FinishBridgePacketBuffer(fbb, root);
  return fbb;
}

const fbs::BridgePacket* ParseBridgePacket(const std::vector<uint8_t>& bytes) {
  return fbs::GetBridgePacket(bytes.data());
}

template <typename T>
auto DequeueOne(CommandQueue& queue) -> std::optional<T> {
  T value = [&]() {
    if constexpr (std::is_constructible_v<T, std::pmr::memory_resource*>) {
      return T(std::pmr::get_default_resource());
    } else {
      return T{};
    }
  }();
  if (queue.TypedStorage<T>().Dequeue(value)) {
    return value;
  }
  return std::nullopt;
}

template <>
auto DequeueOne<QueryGraphCmd>(CommandQueue& queue)
    -> std::optional<QueryGraphCmd> {
  QueryGraphCmd value{};
  if (queue.TypedStorage<QueryGraphCmd>().Dequeue(value)) {
    return value;
  }
  return std::nullopt;
}

}  // namespace

TEST_SUITE("bridge::jni::JniBridge") {
  TEST_CASE("bridge::jni::JniBridge::ctor") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);

    const JniBridge bridge(MakeBridgeConfig(fake_vm));

    CHECK_EQ(bridge.Status(), BridgeStatus::kUninitialized);
    CHECK_EQ(bridge.Config().jvm, fake_vm.Vm());
  }

  TEST_CASE("bridge::jni::JniBridge::~JniBridge") {
    SUBCASE("Destructor destroys callback when JNIEnv is available") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);

      {
        JniBridge bridge(MakeBridgeConfig(fake_vm));
        bridge.RegisterCallback(fake_env.Env(), fake_env.callback_object);
      }

      CHECK_EQ(fake_vm.get_env_calls, 1);
      CHECK_EQ(fake_vm.attach_calls, 0);
      CHECK_EQ(fake_env.delete_global_ref_calls, 1);
    }

    SUBCASE("Destructor skips callback destroy when attach fails") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      fake_vm.get_env_result = JNI_EDETACHED;
      fake_vm.attach_result = JNI_ERR;

      {
        JniBridge bridge(MakeBridgeConfig(fake_vm));
        bridge.RegisterCallback(fake_env.Env(), fake_env.callback_object);
      }

      CHECK_EQ(fake_vm.get_env_calls, 1);
      CHECK_EQ(fake_vm.attach_calls, 1);
      CHECK_EQ(fake_env.delete_global_ref_calls, 0);
    }
  }

  TEST_CASE("bridge::jni::JniBridge::Init") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);
    ScopedAppGuard app_guard;

    auto& app = App::Instance();
    app.Init(AppConfig::From<JniBridge>(MakeBridgeConfig(fake_vm)));

    auto& bridge = app.GetBridge<JniBridge>();
    CHECK_EQ(bridge.Status(), BridgeStatus::kReady);
    CHECK_EQ(bridge.Config().jvm, fake_vm.Vm());

    CHECK(app.IncomingQueue().IsRegistered<QueryGraphCmd>());
    CHECK(app.IncomingQueue().IsRegistered<NodeInfoCmd>());
    CHECK(app.IncomingQueue().IsRegistered<TopicInfoCmd>());
    CHECK(app.IncomingQueue().IsRegistered<ServiceInfoCmd>());
    CHECK(app.IncomingQueue().IsRegistered<InterfaceListCmd>());
    CHECK(app.IncomingQueue().IsRegistered<InterfaceShowCmd>());
    CHECK(app.IncomingQueue().IsRegistered<SubscribeTopicCmd>());
    CHECK(app.IncomingQueue().IsRegistered<PublishMessageCmd>());
    CHECK(app.IncomingQueue().IsRegistered<TopicHzCmd>());
    CHECK(app.IncomingQueue().IsRegistered<TopicBwCmd>());
    CHECK(app.IncomingQueue().IsRegistered<QueryPlayersCmd>());

    CHECK(app.OutgoingQueue().IsRegistered<GraphSnapshotCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<NodeInfoResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<TopicInfoResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ServiceInfoResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<InterfaceListResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<InterfaceShowResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<TopicHzResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<TopicBwResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<PlayerListCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<TopicPayloadCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ErrorCmd>());
  }

  TEST_CASE("bridge::jni::JniBridge::Destroy") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);
    ScopedAppGuard app_guard;

    auto& app = App::Instance();
    app.Init(AppConfig::From<JniBridge>(MakeBridgeConfig(fake_vm)));

    auto& bridge = app.GetBridge<JniBridge>();
    bridge.RegisterCallback(fake_env.Env(), fake_env.callback_object);

    bridge.Destroy(app);

    CHECK_EQ(bridge.Status(), BridgeStatus::kUninitialized);
    CHECK_EQ(fake_env.delete_global_ref_calls, 1);
  }

  TEST_CASE("bridge::jni::JniBridge::Reload") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);
    ScopedAppGuard app_guard;

    auto& app = App::Instance();
    app.Init(AppConfig::From<JniBridge>(MakeBridgeConfig(fake_vm)));

    auto& bridge = app.GetBridge<JniBridge>();
    bridge.Reload(app);

    CHECK_EQ(bridge.Status(), BridgeStatus::kReady);
  }

  TEST_CASE("bridge::jni::JniBridge::Tick") {
    SUBCASE("Tick returns early when callback is not registered") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      ScopedAppGuard app_guard;

      auto& app = App::Instance();
      app.Init(AppConfig::From<JniBridge>(MakeBridgeConfig(fake_vm)));

      auto& bridge = app.GetBridge<JniBridge>();

      GraphSnapshotCmd snapshot(std::pmr::get_default_resource());
      snapshot.request_id = 17U;
      app.OutgoingQueue().Enqueue(std::move(snapshot));

      bridge.Tick(app);

      CHECK(app.OutgoingQueue().HasCommands<GraphSnapshotCmd>());
      CHECK_EQ(fake_env.callback_packets.size(), 0U);
    }

    SUBCASE("Tick drains outgoing packets and clears pending JNI exception") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      fake_env.exception_pending = true;
      ScopedAppGuard app_guard;

      auto& app = App::Instance();
      app.Init(AppConfig::From<JniBridge>(MakeBridgeConfig(fake_vm)));

      auto& bridge = app.GetBridge<JniBridge>();
      bridge.RegisterCallback(fake_env.Env(), fake_env.callback_object);

      GraphSnapshotCmd snapshot(std::pmr::get_default_resource());
      snapshot.request_id = 33U;
      {
        auto& t =
            snapshot.topics.emplace_back(std::pmr::get_default_resource());
        t.name = "/topic/three";
        t.type = "std_msgs/msg/String";
      }
      app.OutgoingQueue().Enqueue(std::move(snapshot));

      bridge.Tick(app);

      CHECK_EQ(app.OutgoingQueue().CommandCount<GraphSnapshotCmd>(), 0U);
      REQUIRE_EQ(fake_env.callback_packets.size(), 1U);

      const auto* packet = ParseBridgePacket(fake_env.callback_packets[0]);
      REQUIRE(packet != nullptr);
      CHECK_EQ(packet->payload_type(), fbs::PacketPayload::GraphSnapshotPacket);
      REQUIRE(packet->payload_as_GraphSnapshotPacket() != nullptr);
      CHECK_EQ(packet->payload_as_GraphSnapshotPacket()->request_id(), 33U);

      CHECK_EQ(fake_env.exception_check_calls, 1);
      CHECK_EQ(fake_env.exception_describe_calls, 1);
      CHECK_EQ(fake_env.exception_clear_calls, 1);
      CHECK_FALSE(fake_env.exception_pending);
    }

    SUBCASE("Tick skips drain when JNIEnv cannot be attached") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      fake_vm.get_env_result = JNI_EDETACHED;
      fake_vm.attach_result = JNI_ERR;
      ScopedAppGuard app_guard;

      auto& app = App::Instance();
      app.Init(AppConfig::From<JniBridge>(MakeBridgeConfig(fake_vm)));

      auto& bridge = app.GetBridge<JniBridge>();
      bridge.RegisterCallback(fake_env.Env(), fake_env.callback_object);

      GraphSnapshotCmd snapshot(std::pmr::get_default_resource());
      snapshot.request_id = 44U;
      app.OutgoingQueue().Enqueue(std::move(snapshot));

      bridge.Tick(app);

      CHECK(app.OutgoingQueue().HasCommands<GraphSnapshotCmd>());
      CHECK_EQ(fake_env.callback_packets.size(), 0U);
      CHECK_EQ(fake_vm.get_env_calls, 1);
      CHECK_EQ(fake_vm.attach_calls, 1);
    }
  }

  TEST_CASE("bridge::jni::JniBridge::RegisterCallback") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);

    JniBridge bridge(MakeBridgeConfig(fake_vm));

    bridge.RegisterCallback(fake_env.Env(), fake_env.callback_object);
    bridge.RegisterCallback(fake_env.Env(), fake_env.callback_object);

    CHECK_EQ(fake_env.new_global_ref_calls, 2);
    CHECK_EQ(fake_env.delete_global_ref_calls, 1);
  }

  TEST_CASE("bridge::jni::JniBridge::ReceivePacket") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);
    ScopedAppGuard app_guard;

    auto& app = App::Instance();
    app.Init(AppConfig::From<JniBridge>(MakeBridgeConfig(fake_vm)));

    auto& bridge = app.GetBridge<JniBridge>();

    SUBCASE("Valid packet is dispatched to incoming queue") {
      auto fbb = BuildQueryGraphPacket(9001U);
      const auto* begin = fbb.GetBufferPointer();
      const auto* end = begin + fbb.GetSize();
      const std::span<const uint8_t> bytes(begin, end);

      bridge.ReceivePacket(bytes);

      const auto cmd = DequeueOne<QueryGraphCmd>(app.IncomingQueue());
      REQUIRE(cmd.has_value());
      CHECK_EQ(cmd->request_id, 9001U);
    }

    SUBCASE("Malformed packet is ignored") {
      const std::array<uint8_t, 3> malformed{1U, 2U, 3U};

      bridge.ReceivePacket(malformed);

      CHECK_FALSE(app.IncomingQueue().HasCommands<QueryGraphCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<SubscribeTopicCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<PublishMessageCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<QueryPlayersCmd>());
    }

    SUBCASE("Unknown payload packet is ignored") {
      auto fbb = BuildGraphSnapshotPacket(11U);
      const auto* begin = fbb.GetBufferPointer();
      const auto* end = begin + fbb.GetSize();
      const std::span<const uint8_t> bytes(begin, end);

      bridge.ReceivePacket(bytes);

      CHECK_FALSE(app.IncomingQueue().HasCommands<QueryGraphCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<SubscribeTopicCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<PublishMessageCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<QueryPlayersCmd>());
    }
  }

  TEST_CASE("bridge::jni::JniBridge::Status") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);

    const JniBridge bridge(MakeBridgeConfig(fake_vm));

    CHECK_EQ(bridge.Status(), BridgeStatus::kUninitialized);
  }

  TEST_CASE("bridge::jni::JniBridge::Config") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);

    const JniBridge bridge(MakeBridgeConfig(fake_vm));

    CHECK_EQ(bridge.Config().jvm, fake_vm.Vm());
    CHECK(bridge.Config().Valid());
  }
}
