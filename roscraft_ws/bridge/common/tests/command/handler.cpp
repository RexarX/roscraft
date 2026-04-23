#include <doctest/doctest.h>

#include <roscraft/bridge/command/handler.hpp>

#include <flatbuffers/flatbuffers.h>

#include <memory_resource>

using namespace roscraft::bridge;

namespace {

struct SimpleHandler {};

struct NonMovableHandler {
  NonMovableHandler() = default;
  NonMovableHandler(const NonMovableHandler&) = default;
  NonMovableHandler(NonMovableHandler&&) = delete;
  ~NonMovableHandler() = default;

  NonMovableHandler& operator=(const NonMovableHandler&) = default;
  NonMovableHandler& operator=(NonMovableHandler&&) = delete;
};

struct PolymorphicHandler {
  virtual ~PolymorphicHandler() = default;
};

struct ReceiveHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::QueryGraphPacket;

  void Receive(CommandQueue&, const fbs::BridgePacket&,
               std::pmr::memory_resource&) {}
};

struct ReceiveHandlerMissingType {
  void Receive(CommandQueue&, const fbs::BridgePacket&,
               std::pmr::memory_resource&) {}
};

struct ReceiveHandlerBadSignature {
  static constexpr auto kReceiveType = fbs::PacketPayload::QueryGraphPacket;

  int Receive(CommandQueue&, const fbs::BridgePacket&,
              std::pmr::memory_resource&) {
    return 0;
  }
};

struct GoodSink {
  void Send(flatbuffers::FlatBufferBuilder&) {}
};

struct BadSink {
  int Send(flatbuffers::FlatBufferBuilder&) { return 0; }
};

struct DrainHandler {
  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue&, Sink&, flatbuffers::FlatBufferBuilder&) {}
};

struct DrainHandlerMissingMethod {};

struct DrainHandlerBadSignature {
  template <PacketSink Sink>
  int DrainAndFlush(CommandQueue&, Sink&, flatbuffers::FlatBufferBuilder&) {
    return 0;
  }
};

}  // namespace

TEST_SUITE("bridge::CommandHandler concepts") {
  TEST_CASE("bridge::CommandHandler") {
    SUBCASE("Simple value type satisfies CommandHandler") {
      CHECK(CommandHandler<SimpleHandler>);
    }

    SUBCASE("Non-movable type does not satisfy CommandHandler") {
      CHECK_FALSE(CommandHandler<NonMovableHandler>);
    }

    SUBCASE("Polymorphic type does not satisfy CommandHandler") {
      CHECK_FALSE(CommandHandler<PolymorphicHandler>);
    }
  }

  TEST_CASE("bridge::CommandHandlerWithReceive") {
    SUBCASE("Valid receive handler satisfies CommandHandlerWithReceive") {
      CHECK(CommandHandlerWithReceive<ReceiveHandler>);
    }

    SUBCASE("Handler without kReceiveType does not satisfy concept") {
      CHECK_FALSE(CommandHandlerWithReceive<ReceiveHandlerMissingType>);
    }

    SUBCASE("Handler with non-void Receive does not satisfy concept") {
      CHECK_FALSE(CommandHandlerWithReceive<ReceiveHandlerBadSignature>);
    }
  }

  TEST_CASE("bridge::PacketSink") {
    SUBCASE("Sink with Send(FlatBufferBuilder&) satisfies PacketSink") {
      CHECK(PacketSink<GoodSink>);
    }

    SUBCASE("Sink with mismatched Send signature does not satisfy PacketSink") {
      CHECK_FALSE(PacketSink<BadSink>);
    }
  }

  TEST_CASE("bridge::CommandHandlerWithDrain") {
    SUBCASE("Valid drain handler satisfies CommandHandlerWithDrain") {
      CHECK(CommandHandlerWithDrain<DrainHandler>);
    }

    SUBCASE("Handler without DrainAndFlush does not satisfy concept") {
      CHECK_FALSE(CommandHandlerWithDrain<DrainHandlerMissingMethod>);
    }

    SUBCASE("Handler with non-void DrainAndFlush does not satisfy concept") {
      CHECK_FALSE(CommandHandlerWithDrain<DrainHandlerBadSignature>);
    }
  }
}
