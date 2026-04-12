#include <pch.hpp>

#include <roscraft/bridge/jni/bridge.hpp>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/jni/command/handlers.hpp>
#include <roscraft/bridge/jni/env_guard.hpp>

#include <flatbuffers/flatbuffers.h>

#include <rclcpp/logging.hpp>

#include <atomic>
#include <cstdint>
#include <span>

namespace roscraft::bridge::jni {

JniBridge::~JniBridge() {
  if (!callback_.Valid()) {
    return;
  }

  JniEnvGuard guard(config_.jvm);
  if (guard.Valid()) {
    callback_.Destroy(guard.Env());
  }
}

void JniBridge::Init(App& app) {
  ROSCRAFT_ASSERT(&app == &App::Instance(),
                  "JniBridge::Init requires App::Instance()!");
  ROSCRAFT_ASSERT(Status() == BridgeStatus::kUninitialized,
                  "JniBridge is already initialized!");

  status_.store(BridgeStatus::kInitializing, std::memory_order_release);

  RCLCPP_INFO(rclcpp::get_logger("JniBridge"), "Initializing JNI bridge...");

  InitCommandHandlerRegistry();

  status_.store(BridgeStatus::kReady, std::memory_order_release);
  RCLCPP_INFO(rclcpp::get_logger("JniBridge"),
              "JNI bridge initialized successfully");
}

void JniBridge::Destroy(App& /*app*/) {
  status_.store(BridgeStatus::kShuttingDown, std::memory_order_release);

  if (callback_.Valid()) {
    JniEnvGuard guard(config_.jvm);
    if (guard.Valid()) {
      callback_.Destroy(guard.Env());
    }
  }

  registry_.Clear();

  status_.store(BridgeStatus::kUninitialized, std::memory_order_release);
}

void JniBridge::Tick(App& /*app*/) {
  if (Status() != BridgeStatus::kReady) [[unlikely]] {
    return;
  }

  if (!callback_.Valid()) [[unlikely]] {
    return;
  }

  JniEnvGuard guard(config_.jvm);
  if (!guard.Valid()) [[unlikely]] {
    RCLCPP_WARN(rclcpp::get_logger("JniBridge"),
                "Tick: failed to obtain JNIEnv — skipping drain.");
    return;
  }

  thread_local flatbuffers::FlatBufferBuilder fbb(4096);

  auto& app = App::Instance();
  registry_.DrainAndDeliverAll<DrainAndDeliverHandlerTypes>(
      app.OutgoingQueue(), guard.Env(), callback_, fbb);

  if (guard.Env()->ExceptionCheck()) {
    guard.Env()->ExceptionDescribe();
    guard.Env()->ExceptionClear();
  }
}

void JniBridge::ReceivePacket(std::span<const uint8_t> packet) {
  ROSCRAFT_ASSERT(Status() == BridgeStatus::kReady,
                  "JniBridge is not initialized!");

  auto& app = App::Instance();

  flatbuffers::Verifier verifier(packet.data(), packet.size());
  if (!fbs::VerifyBridgePacketBuffer(verifier)) [[unlikely]] {
    RCLCPP_WARN(rclcpp::get_logger("JniBridge"),
                "Dropping malformed JNI packet (%zu B)!", packet.size());
    return;
  }

  DispatchReceive(registry_, app.IncomingQueue(),
                  *fbs::GetBridgePacket(packet.data()),
                  app.PendingFrameAllocator());
}

void JniBridge::InitCommandHandlerRegistry() {
  ROSCRAFT_ASSERT(Status() == BridgeStatus::kInitializing,
                  "JniBridge is not initializing!");

  auto& app = App::Instance();
  auto& in = app.IncomingQueue();
  auto& out = app.OutgoingQueue();

  registry_.AddHandler(GraphHandler::From(in, out));
  registry_.AddHandler(SubscribeTopicHandler::From(in));
  registry_.AddHandler(PublishMessageHandler::From(in));
  registry_.AddHandler(PlayerListHandler::From(in, out));
  registry_.AddHandler(TopicPayloadHandler::From(out));
}

}  // namespace roscraft::bridge::jni
