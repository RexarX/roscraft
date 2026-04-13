#include <doctest/doctest.h>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/network/command/handler_registry.hpp>

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

#include <flatbuffers/flatbuffers.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory_resource>
#include <tuple>

using namespace roscraft::bridge;
using namespace roscraft::bridge::network;

namespace {

struct RegistryPlainHandler {
  int value = 0;
};

struct RegistryDrainHandlerA {
  std::reference_wrapper<int> calls;

  void DrainAndSend(CommandQueue&, UdpTransport&,
                    flatbuffers::FlatBufferBuilder&) {
    ++calls.get();
  }
};

struct RegistryDrainHandlerB {
  std::reference_wrapper<int> calls;

  void DrainAndSend(CommandQueue&, UdpTransport&,
                    flatbuffers::FlatBufferBuilder&) {
    ++calls.get();
  }
};

struct RegistryReceiveHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::QueryGraphPacket;

  std::reference_wrapper<int> calls;

  void Receive(CommandQueue&, const fbs::BridgePacket&,
               std::pmr::memory_resource&) {
    ++calls.get();
  }
};

struct RegistryReceiveNoneHandler {
  static constexpr auto kReceiveType = fbs::PacketPayload::NONE;

  std::reference_wrapper<int> calls;

  void Receive(CommandQueue&, const fbs::BridgePacket&,
               std::pmr::memory_resource&) {
    ++calls.get();
  }
};

UdpTransport MakeTransport(asio::ip::udp::socket& socket,
                           std::array<asio::ip::udp::endpoint, 0>& clients) {
  socket.open(asio::ip::udp::v4());
  return UdpTransport(socket, clients);
}

flatbuffers::FlatBufferBuilder BuildQueryGraphPacket(uint64_t request_id) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto inner = fbs::CreateQueryGraphPacket(fbb, request_id);
  const auto root = fbs::CreateBridgePacket(
      fbb, fbs::PacketPayload::QueryGraphPacket, inner.Union());
  fbb.Finish(root);
  return fbb;
}

flatbuffers::FlatBufferBuilder BuildQueryPlayersPacket(uint64_t request_id) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto inner = fbs::CreateQueryPlayersPacket(fbb, request_id);
  const auto root = fbs::CreateBridgePacket(
      fbb, fbs::PacketPayload::QueryPlayersPacket, inner.Union());
  fbb.Finish(root);
  return fbb;
}

}  // namespace

TEST_SUITE("bridge::network::CommandHandlerRegistry") {
  TEST_CASE("bridge::network::CommandHandlerRegistry::ctor") {
    CommandHandlerRegistry registry;

    CHECK(registry.Empty());
    CHECK_EQ(registry.Size(), 0U);
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::AddHandler") {
    CommandHandlerRegistry registry;
    CommandQueue queue;
    flatbuffers::FlatBufferBuilder fbb;
    asio::io_context io_ctx;
    asio::ip::udp::socket socket(io_ctx);
    std::array<asio::ip::udp::endpoint, 0> clients{};
    auto transport = MakeTransport(socket, clients);

    int calls_first = 0;
    int calls_second = 0;

    registry.AddHandler(RegistryDrainHandlerA{calls_first});
    registry.AddHandler(RegistryDrainHandlerA{calls_second});

    registry.DrainAndSend<RegistryDrainHandlerA>(queue, transport, fbb);

    CHECK_EQ(calls_first, 0);
    CHECK_EQ(calls_second, 1);
    CHECK(registry.Contains<RegistryDrainHandlerA>());
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::TryAddHandler") {
    CommandHandlerRegistry registry;
    CommandQueue queue;
    flatbuffers::FlatBufferBuilder fbb;
    asio::io_context io_ctx;
    asio::ip::udp::socket socket(io_ctx);
    std::array<asio::ip::udp::endpoint, 0> clients{};
    auto transport = MakeTransport(socket, clients);

    int calls_first = 0;
    int calls_second = 0;

    const bool first_added =
        registry.TryAddHandler(RegistryDrainHandlerA{calls_first});
    const bool second_added =
        registry.TryAddHandler(RegistryDrainHandlerA{calls_second});

    registry.DrainAndSend<RegistryDrainHandlerA>(queue, transport, fbb);

    CHECK(first_added);
    CHECK_FALSE(second_added);
    CHECK_EQ(calls_first, 1);
    CHECK_EQ(calls_second, 0);

    CHECK(registry.Contains<RegistryDrainHandlerA>());
    CHECK_EQ(registry.Size(), 1U);
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::EmplaceHandler") {
    CommandHandlerRegistry registry;

    int calls = 0;
    auto& handler = registry.EmplaceHandler<RegistryDrainHandlerA>(calls);

    CHECK_EQ(&handler.calls.get(), &calls);
    CHECK(registry.Contains<RegistryDrainHandlerA>());
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::TryEmplaceHandler") {
    CommandHandlerRegistry registry;

    int first_calls = 0;
    int second_calls = 0;

    auto [first_handler, first_inserted] =
        registry.TryEmplaceHandler<RegistryDrainHandlerA>(first_calls);
    auto [second_handler, second_inserted] =
        registry.TryEmplaceHandler<RegistryDrainHandlerA>(second_calls);

    CHECK(first_inserted);
    CHECK_FALSE(second_inserted);
    CHECK_EQ(&first_handler.get(), &second_handler.get());
    CHECK_EQ(&second_handler.get().calls.get(), &first_calls);
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::RemoveHandler") {
    CommandHandlerRegistry registry;

    registry.AddHandler(RegistryPlainHandler{.value = 11});
    CHECK(registry.Contains<RegistryPlainHandler>());

    registry.RemoveHandler<RegistryPlainHandler>();

    CHECK_FALSE(registry.Contains<RegistryPlainHandler>());
    CHECK(registry.Empty());
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::TryRemoveHandler") {
    CommandHandlerRegistry registry;

    registry.AddHandler(RegistryPlainHandler{.value = 42});

    const bool removed_existing =
        registry.TryRemoveHandler<RegistryPlainHandler>();
    const bool removed_missing =
        registry.TryRemoveHandler<RegistryPlainHandler>();

    CHECK(removed_existing);
    CHECK_FALSE(removed_missing);
    CHECK(registry.Empty());
  }

  TEST_CASE(
      "bridge::network::CommandHandlerRegistry::DrainAndSendAll<TupleT>") {
    CommandHandlerRegistry registry;
    CommandQueue queue;
    flatbuffers::FlatBufferBuilder fbb;
    asio::io_context io_ctx;
    asio::ip::udp::socket socket(io_ctx);
    std::array<asio::ip::udp::endpoint, 0> clients{};
    auto transport = MakeTransport(socket, clients);

    int calls_a = 0;
    int calls_b = 0;
    registry.AddHandler(RegistryDrainHandlerA{calls_a});
    registry.AddHandler(RegistryDrainHandlerB{calls_b});

    registry.DrainAndSendAll<
        std::tuple<RegistryDrainHandlerA, RegistryDrainHandlerB>>(
        queue, transport, fbb);

    CHECK_EQ(calls_a, 1);
    CHECK_EQ(calls_b, 1);
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::DrainAndSendAll<Ts...>") {
    CommandHandlerRegistry registry;
    CommandQueue queue;
    flatbuffers::FlatBufferBuilder fbb;
    asio::io_context io_ctx;
    asio::ip::udp::socket socket(io_ctx);
    std::array<asio::ip::udp::endpoint, 0> clients{};
    auto transport = MakeTransport(socket, clients);

    int calls_a = 0;
    int calls_b = 0;
    registry.AddHandler(RegistryDrainHandlerA{calls_a});
    registry.AddHandler(RegistryDrainHandlerB{calls_b});

    registry.DrainAndSendAll<RegistryDrainHandlerA, RegistryDrainHandlerB>(
        queue, transport, fbb);

    CHECK_EQ(calls_a, 1);
    CHECK_EQ(calls_b, 1);
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::DrainAndSend") {
    CommandHandlerRegistry registry;
    CommandQueue queue;
    flatbuffers::FlatBufferBuilder fbb;
    asio::io_context io_ctx;
    asio::ip::udp::socket socket(io_ctx);
    std::array<asio::ip::udp::endpoint, 0> clients{};
    auto transport = MakeTransport(socket, clients);

    int calls = 0;
    registry.AddHandler(RegistryDrainHandlerA{calls});

    registry.DrainAndSend<RegistryDrainHandlerA>(queue, transport, fbb);

    CHECK_EQ(calls, 1);

    registry.DrainAndSend<RegistryDrainHandlerA>(queue, transport, fbb);
    CHECK_EQ(calls, 2);
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::DrainAndSendIfExists") {
    CommandHandlerRegistry registry;
    CommandQueue queue;
    flatbuffers::FlatBufferBuilder fbb;
    asio::io_context io_ctx;
    asio::ip::udp::socket socket(io_ctx);
    std::array<asio::ip::udp::endpoint, 0> clients{};
    auto transport = MakeTransport(socket, clients);

    int calls = 0;

    registry.DrainAndSendIfExists<RegistryDrainHandlerA>(queue, transport, fbb);
    CHECK_EQ(calls, 0);

    registry.AddHandler(RegistryDrainHandlerA{calls});
    registry.DrainAndSendIfExists<RegistryDrainHandlerA>(queue, transport, fbb);
    CHECK_EQ(calls, 1);
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::Receive") {
    CommandHandlerRegistry registry;
    CommandQueue queue;
    std::pmr::monotonic_buffer_resource arena;

    int calls = 0;
    registry.AddHandler(RegistryReceiveHandler{calls});

    const auto query_graph_fbb = BuildQueryGraphPacket(1U);
    const auto query_players_fbb = BuildQueryPlayersPacket(2U);

    const auto* query_graph_pkt =
        fbs::GetBridgePacket(query_graph_fbb.GetBufferPointer());
    const auto* query_players_pkt =
        fbs::GetBridgePacket(query_players_fbb.GetBufferPointer());

    registry.Receive<RegistryReceiveHandler>(queue, *query_graph_pkt, arena);
    registry.Receive<RegistryReceiveHandler>(queue, *query_players_pkt, arena);
    registry.Receive<RegistryReceiveHandler>(queue, *query_graph_pkt, arena);

    CHECK_EQ(calls, 2);

    int none_calls = 0;
    registry.AddHandler(RegistryReceiveNoneHandler{none_calls});
    registry.Receive<RegistryReceiveNoneHandler>(queue, *query_graph_pkt,
                                                 arena);
    CHECK_EQ(none_calls, 0);
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::Contains") {
    CommandHandlerRegistry registry;
    CHECK_FALSE(registry.Contains<RegistryPlainHandler>());

    registry.AddHandler(RegistryPlainHandler{.value = 5});
    CHECK(registry.Contains<RegistryPlainHandler>());
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::Empty") {
    CommandHandlerRegistry registry;
    CHECK(registry.Empty());

    registry.AddHandler(RegistryPlainHandler{.value = 9});
    CHECK_FALSE(registry.Empty());
  }

  TEST_CASE("bridge::network::CommandHandlerRegistry::Size") {
    CommandHandlerRegistry registry;
    CHECK_EQ(registry.Size(), 0U);

    registry.AddHandler(RegistryPlainHandler{.value = 1});
    CHECK_EQ(registry.Size(), 1U);

    int calls = 0;
    registry.AddHandler(RegistryDrainHandlerA{calls});
    CHECK_EQ(registry.Size(), 2U);
  }
}
