#include <doctest/doctest.h>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/jni/command/callback.hpp>
#include <roscraft/bridge/jni/command/handler.hpp>

#include <flatbuffers/flatbuffers.h>

#include <concepts>
#include <cstdint>
#include <memory_resource>

using namespace roscraft::bridge;
using namespace roscraft::bridge::jni;

namespace {

struct PlainCommandHandler {};

struct ReceiveHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::QueryGraphPacket;

  void Receive(CommandQueue&, const fbs::BridgePacket&,
               std::pmr::memory_resource&) {}
};

struct MissingReceiveTypeHandler {
  void Receive(CommandQueue&, const fbs::BridgePacket&,
               std::pmr::memory_resource&) {}
};

struct WrongReceiveSignatureHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::QueryGraphPacket;

  int Receive(CommandQueue&, const fbs::BridgePacket&,
              std::pmr::memory_resource&) {
    return 0;
  }
};

struct DrainAndDeliverHandler {
  void DrainAndDeliver(CommandQueue&, JNIEnv*, const BridgeCallback&,
                       flatbuffers::FlatBufferBuilder&) {}
};

struct WrongDrainAndDeliverSignatureHandler {
  int DrainAndDeliver(CommandQueue&, JNIEnv*, const BridgeCallback&,
                      flatbuffers::FlatBufferBuilder&) {
    return 0;
  }
};

}  // namespace

TEST_SUITE("bridge::jni::CommandHandler") {
  TEST_CASE("bridge::jni::CommandHandler") {
    CHECK(CommandHandler<PlainCommandHandler>);
    CHECK(CommandHandler<int&>);
    CHECK_FALSE(CommandHandler<void>);
  }

  TEST_CASE("bridge::jni::CommandHandlerWithReceive") {
    CHECK(CommandHandlerWithReceive<ReceiveHandler>);
    CHECK_FALSE(CommandHandlerWithReceive<MissingReceiveTypeHandler>);
    CHECK_FALSE(CommandHandlerWithReceive<WrongReceiveSignatureHandler>);
    CHECK_FALSE(CommandHandlerWithReceive<int>);
  }

  TEST_CASE("bridge::jni::CommandHandlerWithDrainAndDeliver") {
    CHECK(CommandHandlerWithDrainAndDeliver<DrainAndDeliverHandler>);
    CHECK_FALSE(CommandHandlerWithDrainAndDeliver<
                WrongDrainAndDeliverSignatureHandler>);
    CHECK_FALSE(CommandHandlerWithDrainAndDeliver<int>);
  }
}
