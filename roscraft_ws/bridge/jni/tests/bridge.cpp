#include <doctest/doctest.h>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/app/config.hpp>
#include <roscraft/bridge/bridge.hpp>
#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/jni/bridge.hpp>
#include <roscraft/generated/bridge_packets.hpp>

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

flatbuffers::FlatBufferBuilder BuildServiceCallPacket(uint64_t request_id) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto service_name = fbb.CreateString("/service");
  const auto service_type = fbb.CreateString("unsupported/srv/Type");
  const std::array<uint8_t, 3> payload{9U, 8U, 7U};
  const auto payload_offset = fbb.CreateVector(payload.data(), payload.size());
  const auto inner =
      fbs::CreateServiceCallPacket(fbb, request_id, service_name, service_type,
                                   payload_offset, 0.25, 0U, 0.0);
  const auto root = fbs::CreateBridgePacket(
      fbb, fbs::PacketPayload::ServiceCallPacket, inner.Union());
  fbs::FinishBridgePacketBuffer(fbb, root);
  return fbb;
}

flatbuffers::FlatBufferBuilder BuildParamListPacket(uint64_t request_id) {
  flatbuffers::FlatBufferBuilder fbb;
  std::vector<flatbuffers::Offset<flatbuffers::String>> prefixes;
  prefixes.push_back(fbb.CreateString("/robot"));
  const auto inner = fbs::CreateParamListPacketDirect(
      fbb, request_id, "", &prefixes, 2U, true, "^foo", 0.25);
  const auto root = fbs::CreateBridgePacket(
      fbb, fbs::PacketPayload::ParamListPacket, inner.Union());
  fbs::FinishBridgePacketBuffer(fbb, root);
  return fbb;
}

flatbuffers::FlatBufferBuilder BuildActionSendGoalPacket(uint64_t request_id) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto action_name = fbb.CreateString("/demo/action");
  const auto action_type = fbb.CreateString("unsupported/action/Type");
  const std::array<uint8_t, 3> goal_payload{9U, 8U, 7U};
  const auto goal_payload_offset =
      fbb.CreateVector(goal_payload.data(), goal_payload.size());
  const auto inner =
      fbs::CreateActionSendGoalPacket(fbb, request_id, action_name, action_type,
                                      goal_payload_offset, true, 0.25);
  const auto root = fbs::CreateBridgePacket(
      fbb, fbs::PacketPayload::ActionSendGoalPacket, inner.Union());
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
    CHECK(app.IncomingQueue().IsRegistered<TopicSubscribeCmd>());
    CHECK(app.IncomingQueue().IsRegistered<TopicPublishMessageCmd>());
    CHECK(app.IncomingQueue().IsRegistered<QueryPlayersCmd>());
    CHECK(app.IncomingQueue().IsRegistered<TopicHzCmd>());
    CHECK(app.IncomingQueue().IsRegistered<TopicBwCmd>());
    CHECK(app.IncomingQueue().IsRegistered<TopicDelayCmd>());
    CHECK(app.IncomingQueue().IsRegistered<ServiceCallCmd>());
    CHECK(app.IncomingQueue().IsRegistered<ParamListCmd>());
    CHECK(app.IncomingQueue().IsRegistered<ParamGetCmd>());
    CHECK(app.IncomingQueue().IsRegistered<ParamSetCmd>());
    CHECK(app.IncomingQueue().IsRegistered<ParamDescribeCmd>());
    CHECK(app.IncomingQueue().IsRegistered<ParamDumpCmd>());
    CHECK(app.IncomingQueue().IsRegistered<ParamLoadCmd>());
    CHECK(app.IncomingQueue().IsRegistered<ActionInfoCmd>());
    CHECK(app.IncomingQueue().IsRegistered<ActionSendGoalCmd>());

    CHECK(app.OutgoingQueue().IsRegistered<GraphSnapshotCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<NodeInfoResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<TopicInfoResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ServiceInfoResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<InterfaceListResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<InterfaceShowResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<TopicPayloadCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<TopicHzResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<TopicBwResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<TopicDelayResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ServiceCallResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ParamListResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ParamGetResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ParamSetResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ParamDescribeResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ParamDumpResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ParamLoadResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ActionInfoResponseCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ActionFeedbackCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<ActionResultCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<PlayerListCmd>());
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

    SUBCASE("Tick drains service, param, and action response packets") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      ScopedAppGuard app_guard;

      auto& app = App::Instance();
      app.Init(AppConfig::From<JniBridge>(MakeBridgeConfig(fake_vm)));

      auto& bridge = app.GetBridge<JniBridge>();
      bridge.RegisterCallback(fake_env.Env(), fake_env.callback_object);

      ServiceCallResponseCmd service_cmd(std::pmr::get_default_resource());
      service_cmd.request_id = 71U;
      service_cmd.service_name = "/service";
      service_cmd.service_type = "std_srvs/srv/Trigger";
      service_cmd.success = true;
      service_cmd.response_payload = {9U, 8U};
      service_cmd.result_text = "ok";
      app.OutgoingQueue().Enqueue(std::move(service_cmd));

      ParamListResponseCmd param_cmd(std::pmr::get_default_resource());
      param_cmd.request_id = 72U;
      param_cmd.node_name = "/node";
      param_cmd.names.emplace_back("foo");
      param_cmd.prefixes.emplace_back("/robot");
      param_cmd.types.emplace_back("integer");
      app.OutgoingQueue().Enqueue(std::move(param_cmd));

      ActionResultCmd action_cmd(std::pmr::get_default_resource());
      action_cmd.request_id = 73U;
      action_cmd.action_name = "/demo/action";
      action_cmd.action_type = "example_interfaces/action/Fibonacci";
      action_cmd.success = true;
      action_cmd.result_payload = {1U, 2U, 3U};
      action_cmd.result_text = "goal finished";
      app.OutgoingQueue().Enqueue(std::move(action_cmd));

      bridge.Tick(app);

      CHECK_EQ(app.OutgoingQueue().CommandCount<ServiceCallResponseCmd>(), 0U);
      CHECK_EQ(app.OutgoingQueue().CommandCount<ParamListResponseCmd>(), 0U);
      CHECK_EQ(app.OutgoingQueue().CommandCount<ActionResultCmd>(), 0U);
      REQUIRE_EQ(fake_env.callback_packets.size(), 3U);

      bool saw_service = false;
      bool saw_param = false;
      bool saw_action = false;
      for (const auto& bytes : fake_env.callback_packets) {
        const auto* packet = ParseBridgePacket(bytes);
        REQUIRE(packet != nullptr);

        switch (packet->payload_type()) {
          case fbs::PacketPayload::ServiceCallResponsePacket:
            saw_service = true;
            CHECK_EQ(
                packet->payload_as_ServiceCallResponsePacket()->request_id(),
                71U);
            break;
          case fbs::PacketPayload::ParamListResponsePacket:
            saw_param = true;
            CHECK_EQ(packet->payload_as_ParamListResponsePacket()->request_id(),
                     72U);
            break;
          case fbs::PacketPayload::ActionResultPacket:
            saw_action = true;
            CHECK_EQ(packet->payload_as_ActionResultPacket()->request_id(),
                     73U);
            break;
          default:
            break;
        }
      }

      CHECK(saw_service);
      CHECK(saw_param);
      CHECK(saw_action);
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
      CHECK_FALSE(app.IncomingQueue().HasCommands<TopicSubscribeCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<TopicPublishMessageCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<ServiceCallCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<ParamListCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<ActionSendGoalCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<QueryPlayersCmd>());
    }

    SUBCASE("Unknown payload packet is ignored") {
      auto fbb = BuildGraphSnapshotPacket(11U);
      const auto* begin = fbb.GetBufferPointer();
      const auto* end = begin + fbb.GetSize();
      const std::span<const uint8_t> bytes(begin, end);

      bridge.ReceivePacket(bytes);

      CHECK_FALSE(app.IncomingQueue().HasCommands<QueryGraphCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<TopicSubscribeCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<TopicPublishMessageCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<ServiceCallCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<ParamListCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<ActionSendGoalCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<QueryPlayersCmd>());
    }

    SUBCASE("Service, param, and action packets are dispatched") {
      auto service_fbb = BuildServiceCallPacket(9201U);
      auto param_fbb = BuildParamListPacket(9202U);
      auto action_fbb = BuildActionSendGoalPacket(9203U);

      const std::span<const uint8_t> service_bytes(
          service_fbb.GetBufferPointer(), service_fbb.GetSize());
      const std::span<const uint8_t> param_bytes(param_fbb.GetBufferPointer(),
                                                 param_fbb.GetSize());
      const std::span<const uint8_t> action_bytes(action_fbb.GetBufferPointer(),
                                                  action_fbb.GetSize());

      bridge.ReceivePacket(service_bytes);
      bridge.ReceivePacket(param_bytes);
      bridge.ReceivePacket(action_bytes);

      ServiceCallCmd service_cmd(std::pmr::get_default_resource());
      CHECK(app.IncomingQueue().TypedStorage<ServiceCallCmd>().Dequeue(
          service_cmd));
      CHECK_EQ(service_cmd.request_id, 9201U);
      CHECK_EQ(service_cmd.service_name, "/service");
      CHECK_EQ(service_cmd.service_type, "unsupported/srv/Type");
      CHECK_EQ(service_cmd.payload.size(), 3U);
      CHECK_EQ(service_cmd.payload[0], 9U);
      CHECK_EQ(service_cmd.payload[2], 7U);
      CHECK_EQ(service_cmd.timeout_seconds, doctest::Approx(0.25));
      CHECK_EQ(service_cmd.repeat_count, 0U);
      CHECK_EQ(service_cmd.rate_hz, doctest::Approx(0.0));

      ParamListCmd param_cmd(std::pmr::get_default_resource());
      CHECK(
          app.IncomingQueue().TypedStorage<ParamListCmd>().Dequeue(param_cmd));
      CHECK_EQ(param_cmd.request_id, 9202U);
      CHECK_EQ(param_cmd.node_name, "");
      CHECK_EQ(param_cmd.prefixes.size(), 1U);
      CHECK_EQ(param_cmd.prefixes[0], "/robot");
      CHECK_EQ(param_cmd.depth, 2U);
      CHECK(param_cmd.include_types);
      CHECK_EQ(param_cmd.filter_regex, "^foo");
      CHECK_EQ(param_cmd.timeout_seconds, doctest::Approx(0.25));

      ActionSendGoalCmd action_cmd(std::pmr::get_default_resource());
      CHECK(app.IncomingQueue().TypedStorage<ActionSendGoalCmd>().Dequeue(
          action_cmd));
      CHECK_EQ(action_cmd.request_id, 9203U);
      CHECK_EQ(action_cmd.action_name, "/demo/action");
      CHECK_EQ(action_cmd.action_type, "unsupported/action/Type");
      CHECK_EQ(action_cmd.goal_payload.size(), 3U);
      CHECK_EQ(action_cmd.goal_payload[0], 9U);
      CHECK_EQ(action_cmd.goal_payload[2], 7U);
      CHECK(action_cmd.feedback);
      CHECK_EQ(action_cmd.timeout_seconds, doctest::Approx(0.25));
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
