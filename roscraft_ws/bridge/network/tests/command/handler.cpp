#include <doctest/doctest.h>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/network/command/handler.hpp>

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

#include <flatbuffers/flatbuffers.h>

#include <cstddef>
#include <cstdint>
#include <memory_resource>

using namespace roscraft::bridge;
using namespace roscraft::bridge::network;

namespace {

struct PlainCommandHandler {};

struct PolymorphicCommandHandler {
  virtual ~PolymorphicCommandHandler() = default;
};

struct alignas(64) OverAlignedCommandHandler {
  uint8_t value = 0;
};

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

struct DrainAndSendHandler {
  void DrainAndSend(CommandQueue&, UdpTransport&,
                    flatbuffers::FlatBufferBuilder&) {}
};

struct WrongDrainAndSendSignatureHandler {
  int DrainAndSend(CommandQueue&, UdpTransport&,
                   flatbuffers::FlatBufferBuilder&) {
    return 0;
  }
};

}  // namespace

TEST_SUITE("bridge::network::CommandHandler") {
  TEST_CASE("bridge::network::CommandHandler") {
    CHECK(CommandHandler<PlainCommandHandler>);
    CHECK_FALSE(CommandHandler<PolymorphicCommandHandler>);
    CHECK_FALSE(CommandHandler<OverAlignedCommandHandler>);
  }

  TEST_CASE("bridge::network::CommandHandlerWithReceive") {
    CHECK(CommandHandlerWithReceive<ReceiveHandler>);
    CHECK_FALSE(CommandHandlerWithReceive<MissingReceiveTypeHandler>);
    CHECK_FALSE(CommandHandlerWithReceive<WrongReceiveSignatureHandler>);
    CHECK_FALSE(CommandHandlerWithReceive<int>);
  }

  TEST_CASE("bridge::network::CommandHandlerWithDrainAndSend") {
    CHECK(CommandHandlerWithDrainAndSend<DrainAndSendHandler>);
    CHECK_FALSE(
        CommandHandlerWithDrainAndSend<WrongDrainAndSendSignatureHandler>);
    CHECK_FALSE(CommandHandlerWithDrainAndSend<int>);
  }
}
