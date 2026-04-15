#include <doctest/doctest.h>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/app/config.hpp>
#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/jni/bridge.hpp>
#include <roscraft/bridge/jni/native_methods.hpp>

#include <flatbuffers/flatbuffers.h>

#include <array>
#include <cstdint>
#include <memory_resource>
#include <optional>
#include <span>

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

flatbuffers::FlatBufferBuilder BuildQueryGraphPacket(uint64_t request_id) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto payload = fbs::CreateQueryGraphPacket(fbb, request_id);
  const auto packet = fbs::CreateBridgePacket(
      fbb, fbs::PacketPayload::QueryGraphPacket, payload.Union());
  fbs::FinishBridgePacketBuffer(fbb, packet);
  return fbb;
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

TEST_SUITE("bridge::jni::native_methods") {
  TEST_CASE("Java_net_roscraft_bridge_JniBridge_nativeCreate") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);
    ScopedAppGuard app_guard;
    auto& app = App::Instance();

    const auto created = Java_net_roscraft_bridge_JniBridge_nativeCreate(
        fake_env.Env(), nullptr);

    CHECK_EQ(created, JNI_TRUE);
    CHECK_EQ(fake_env.get_java_vm_calls, 1);
    CHECK_EQ(app.State(), AppState::kInitialized);

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

  TEST_CASE("Java_net_roscraft_bridge_JniBridge_nativeDestroy") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);
    ScopedAppGuard app_guard;
    auto& app = App::Instance();

    REQUIRE_EQ(Java_net_roscraft_bridge_JniBridge_nativeCreate(fake_env.Env(),
                                                               nullptr),
               JNI_TRUE);
    REQUIRE_EQ(app.State(), AppState::kInitialized);

    Java_net_roscraft_bridge_JniBridge_nativeDestroy(fake_env.Env(), nullptr);

    CHECK_EQ(app.State(), AppState::kUninitialized);
  }

  TEST_CASE("Java_net_roscraft_bridge_JniBridge_nativeRegisterCallback") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);
    ScopedAppGuard app_guard;

    REQUIRE_EQ(Java_net_roscraft_bridge_JniBridge_nativeCreate(fake_env.Env(),
                                                               nullptr),
               JNI_TRUE);

    Java_net_roscraft_bridge_JniBridge_nativeRegisterCallback(
        fake_env.Env(), nullptr, fake_env.callback_object);

    CHECK_EQ(fake_env.new_global_ref_calls, 1);
    CHECK_EQ(fake_env.get_method_id_calls, 1);
  }

  TEST_CASE("Java_net_roscraft_bridge_JniBridge_nativeTick") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);
    ScopedAppGuard app_guard;
    auto& app = App::Instance();

    REQUIRE_EQ(Java_net_roscraft_bridge_JniBridge_nativeCreate(fake_env.Env(),
                                                               nullptr),
               JNI_TRUE);

    auto& bridge = app.GetBridge<JniBridge>();
    bridge.RegisterCallback(fake_env.Env(), fake_env.callback_object);

    GraphSnapshotCmd snapshot(std::pmr::get_default_resource());
    snapshot.request_id = 500U;
    {
      auto& t = snapshot.topics.emplace_back(std::pmr::get_default_resource());
      t.name = "/native/tick";
      t.type = "std_msgs/msg/String";
    }
    app.OutgoingQueue().Enqueue(std::move(snapshot));

    Java_net_roscraft_bridge_JniBridge_nativeTick(fake_env.Env(), nullptr);

    CHECK_EQ(app.OutgoingQueue().CommandCount<GraphSnapshotCmd>(), 0U);
    CHECK_EQ(fake_env.callback_packets.size(), 1U);
  }

  TEST_CASE("Java_net_roscraft_bridge_JniBridge_nativeSendPacket") {
    tests::FakeJniEnv fake_env;
    tests::FakeJavaVM fake_vm;
    fake_vm.Bind(fake_env);
    ScopedAppGuard app_guard;
    auto& app = App::Instance();

    REQUIRE_EQ(Java_net_roscraft_bridge_JniBridge_nativeCreate(fake_env.Env(),
                                                               nullptr),
               JNI_TRUE);

    auto fbb = BuildQueryGraphPacket(700U);
    const auto* begin = fbb.GetBufferPointer();
    const auto* end = begin + fbb.GetSize();
    const std::span<const uint8_t> bytes(begin, end);
    const auto packet = fake_env.MakeByteArray(bytes);

    Java_net_roscraft_bridge_JniBridge_nativeSendPacket(fake_env.Env(), nullptr,
                                                        packet);

    const auto cmd = DequeueOne<QueryGraphCmd>(app.IncomingQueue());
    REQUIRE(cmd.has_value());
    CHECK_EQ(cmd->request_id, 700U);
    CHECK_EQ(fake_env.release_byte_array_elements_calls, 1);
    CHECK_EQ(fake_env.last_release_mode, JNI_ABORT);
  }
}
