#include <doctest/doctest.h>

#include <roscraft/bridge/command/handler_registry.hpp>

#include "handlers/test_utils.hpp"

#include <flatbuffers/flatbuffers.h>

#include <memory_resource>
#include <tuple>

using namespace roscraft::bridge;

namespace {

struct RegistryReceiveCmd {
  uint64_t request_id = 0;
};

struct RegistryDrainCmd {
  uint64_t request_id = 0;
};

struct ValueHandler {
  explicit ValueHandler(int initial_value) : value(initial_value) {}

  int value = 0;
};

struct RegistryReceiveHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::QueryGraphPacket;

  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource&) {
    const auto* inner = pkt.payload_as_QueryGraphPacket();
    in.Enqueue(producer, RegistryReceiveCmd{.request_id = inner->request_id()});
  }

  [[nodiscard]] static RegistryReceiveHandler From(CommandQueue& in) {
    return {in.MakeProducerToken<RegistryReceiveCmd>()};
  }

  CommandQueueProducerToken producer;
};

struct RegistryDrainHandler {
  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb) {
    auto& storage = out.TypedStorage<RegistryDrainCmd>();

    RegistryDrainCmd cmd;
    while (storage.Dequeue(consumer, cmd)) {
      fbb.Clear();

      const auto inner = fbs::CreateQueryPlayersPacket(fbb, cmd.request_id);
      const auto root = fbs::CreateBridgePacket(
          fbb, fbs::PacketPayload::QueryPlayersPacket, inner.Union());
      fbs::FinishBridgePacketBuffer(fbb, root);

      sink.Send(fbb);
    }
  }

  [[nodiscard]] static RegistryDrainHandler From(CommandQueue& out) {
    return {out.MakeConsumerToken<RegistryDrainCmd>()};
  }

  CommandQueueConsumerToken consumer;
};

struct RegistryOtherDrainHandler {
  template <PacketSink Sink>
  void DrainAndFlush(CommandQueue&, Sink&, flatbuffers::FlatBufferBuilder&) {}

  [[nodiscard]] static RegistryOtherDrainHandler From(CommandQueue& out) {
    return {out.MakeConsumerToken<RegistryDrainCmd>()};
  }

  CommandQueueConsumerToken consumer;
};

[[nodiscard]] auto BuildQueryGraphPacket(uint64_t request_id)
    -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::QueryGraphPacket,
                            [request_id](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateQueryGraphPacket(fbb,
                                                                 request_id);
                            });
}

[[nodiscard]] auto BuildQueryPlayersPacket(uint64_t request_id)
    -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::QueryPlayersPacket,
                            [request_id](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateQueryPlayersPacket(fbb,
                                                                   request_id);
                            });
}

}  // namespace

TEST_SUITE("bridge::CommandHandlerRegistry") {
  TEST_CASE("bridge::CommandHandlerRegistry::ctor") {
    CommandHandlerRegistry registry;

    CHECK(registry.Empty());
    CHECK_EQ(registry.Size(), 0U);
  }

  TEST_CASE("bridge::CommandHandlerRegistry::AddHandler") {
    CommandHandlerRegistry registry;

    registry.AddHandler(ValueHandler{7});

    CHECK(registry.Contains<ValueHandler>());
    CHECK_EQ(registry.Size(), 1U);
  }

  TEST_CASE("bridge::CommandHandlerRegistry::TryAddHandler") {
    CommandHandlerRegistry registry;

    const bool inserted_first = registry.TryAddHandler(ValueHandler{11});
    const bool inserted_second = registry.TryAddHandler(ValueHandler{22});

    CHECK(inserted_first);
    CHECK_FALSE(inserted_second);
    CHECK(registry.Contains<ValueHandler>());
    CHECK_EQ(registry.Size(), 1U);
  }

  TEST_CASE("bridge::CommandHandlerRegistry::EmplaceHandler") {
    CommandHandlerRegistry registry;

    ValueHandler& handler = registry.EmplaceHandler<ValueHandler>(33);

    CHECK_EQ(handler.value, 33);
    CHECK(registry.Contains<ValueHandler>());
    CHECK_EQ(registry.Size(), 1U);
  }

  TEST_CASE("bridge::CommandHandlerRegistry::TryEmplaceHandler") {
    CommandHandlerRegistry registry;

    auto [first_ref, first_inserted] =
        registry.TryEmplaceHandler<ValueHandler>(44);
    auto [second_ref, second_inserted] =
        registry.TryEmplaceHandler<ValueHandler>(55);

    CHECK(first_inserted);
    CHECK_FALSE(second_inserted);
    CHECK_EQ(first_ref.get().value, 44);
    CHECK_EQ(second_ref.get().value, 44);
    CHECK_EQ(registry.Size(), 1U);
  }

  TEST_CASE("bridge::CommandHandlerRegistry::RemoveHandler") {
    CommandHandlerRegistry registry;
    registry.AddHandler(ValueHandler{66});

    registry.RemoveHandler<ValueHandler>();

    CHECK_FALSE(registry.Contains<ValueHandler>());
    CHECK(registry.Empty());
  }

  TEST_CASE("bridge::CommandHandlerRegistry::TryRemoveHandler") {
    CommandHandlerRegistry registry;

    SUBCASE("Returns false for missing handler") {
      CHECK_FALSE(registry.TryRemoveHandler<ValueHandler>());
    }

    SUBCASE("Returns true when handler exists") {
      registry.AddHandler(ValueHandler{77});

      CHECK(registry.TryRemoveHandler<ValueHandler>());
      CHECK_FALSE(registry.Contains<ValueHandler>());
    }
  }

  TEST_CASE("bridge::CommandHandlerRegistry::Clear") {
    CommandHandlerRegistry registry;
    registry.AddHandler(ValueHandler{88});
    registry.AddHandler(ValueHandler{99});

    registry.Clear();

    CHECK(registry.Empty());
    CHECK_EQ(registry.Size(), 0U);
  }

  TEST_CASE("bridge::CommandHandlerRegistry::Receive") {
    CommandQueue in;
    in.Register<RegistryReceiveCmd>();

    CommandHandlerRegistry registry;
    registry.AddHandler(RegistryReceiveHandler::From(in));

    SUBCASE("Dispatches matching packet to registered receive handler") {
      const auto bytes = BuildQueryGraphPacket(101U);
      const auto* packet = tests::ParsePacket(bytes);
      REQUIRE(packet != nullptr);

      registry.Receive<RegistryReceiveHandler>(
          in, *packet, *std::pmr::get_default_resource());

      RegistryReceiveCmd cmd{};
      CHECK(in.TypedStorage<RegistryReceiveCmd>().Dequeue(cmd));
      CHECK_EQ(cmd.request_id, 101U);
    }

    SUBCASE("Ignores non-matching payload type") {
      const auto bytes = BuildQueryPlayersPacket(202U);
      const auto* packet = tests::ParsePacket(bytes);
      REQUIRE(packet != nullptr);

      registry.Receive<RegistryReceiveHandler>(
          in, *packet, *std::pmr::get_default_resource());

      CHECK_FALSE(in.HasCommands<RegistryReceiveCmd>());
    }
  }

  TEST_CASE("bridge::CommandHandlerRegistry::DrainAndFlush") {
    CommandQueue out;
    out.Register<RegistryDrainCmd>();

    CommandHandlerRegistry registry;
    registry.AddHandler(RegistryDrainHandler::From(out));

    out.Enqueue(RegistryDrainCmd{.request_id = 303U});

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    registry.DrainAndFlush<RegistryDrainHandler>(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::QueryPlayersPacket);
    REQUIRE(packet->payload_as_QueryPlayersPacket() != nullptr);
    CHECK_EQ(packet->payload_as_QueryPlayersPacket()->request_id(), 303U);
    CHECK_EQ(out.CommandCount<RegistryDrainCmd>(), 0U);
  }

  TEST_CASE("bridge::CommandHandlerRegistry::DrainAndFlushIfExists") {
    CommandQueue out;
    out.Register<RegistryDrainCmd>();
    out.Enqueue(RegistryDrainCmd{.request_id = 404U});

    CommandHandlerRegistry registry;

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    registry.DrainAndFlushIfExists<RegistryDrainHandler>(out, sink, fbb);

    CHECK_EQ(sink.packets.size(), 0U);
    CHECK_EQ(out.CommandCount<RegistryDrainCmd>(), 1U);
  }

  TEST_CASE("bridge::CommandHandlerRegistry::DrainAndFlushAll") {
    CommandQueue out;
    out.Register<RegistryDrainCmd>();

    CommandHandlerRegistry registry;
    registry.AddHandler(RegistryDrainHandler::From(out));

    out.Enqueue(RegistryDrainCmd{.request_id = 505U});

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    registry.DrainAndFlushAll<
        std::tuple<RegistryDrainHandler, RegistryOtherDrainHandler>>(out, sink,
                                                                     fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::QueryPlayersPacket);
    CHECK_EQ(out.CommandCount<RegistryDrainCmd>(), 0U);
  }

  TEST_CASE("bridge::CommandHandlerRegistry::DrainAndFlushAll pack overload") {
    CommandQueue out;
    out.Register<RegistryDrainCmd>();

    CommandHandlerRegistry registry;
    registry.AddHandler(RegistryDrainHandler::From(out));

    out.Enqueue(RegistryDrainCmd{.request_id = 606U});

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    registry.DrainAndFlushAll<RegistryDrainHandler, RegistryOtherDrainHandler>(
        out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::QueryPlayersPacket);
    CHECK_EQ(out.CommandCount<RegistryDrainCmd>(), 0U);
  }
}
