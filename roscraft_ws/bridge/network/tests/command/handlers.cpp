#include <doctest/doctest.h>

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/network/command/handlers.hpp>

#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/udp.hpp>

#include <flatbuffers/flatbuffers.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory_resource>
#include <optional>
#include <span>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

using namespace roscraft::bridge;
using namespace roscraft::bridge::network;

namespace {

void RegisterIncomingQueueTypes(CommandQueue& queue) {
  queue.Register<QueryGraphCmd>();
  queue.Register<SubscribeTopicCmd>();
  queue.Register<PublishMessageCmd>();
  queue.Register<QueryPlayersCmd>();
}

void RegisterOutgoingQueueTypes(CommandQueue& queue) {
  queue.Register<GraphSnapshotCmd>();
  queue.Register<TopicPayloadCmd>();
  queue.Register<PlayerListCmd>();
}

const fbs::BridgePacket* BuildBridgePacket(flatbuffers::FlatBufferBuilder& fbb,
                                           flatbuffers::Offset<void> payload,
                                           fbs::PacketPayload payload_type) {
  const auto root = fbs::CreateBridgePacket(fbb, payload_type, payload);
  fbb.Finish(root);
  return fbs::GetBridgePacket(fbb.GetBufferPointer());
}

flatbuffers::FlatBufferBuilder BuildQueryGraphPacket(uint64_t request_id) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto inner = fbs::CreateQueryGraphPacket(fbb, request_id);
  static_cast<void>(BuildBridgePacket(fbb, inner.Union(),
                                      fbs::PacketPayload::QueryGraphPacket));
  return fbb;
}

flatbuffers::FlatBufferBuilder BuildSubscribeTopicPacket(
    uint64_t request_id, std::string_view topic_name,
    std::string_view message_type) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto inner = fbs::CreateSubscribeTopicPacket(
      fbb, request_id, fbb.CreateString(topic_name.data(), topic_name.size()),
      fbb.CreateString(message_type.data(), message_type.size()));
  static_cast<void>(BuildBridgePacket(
      fbb, inner.Union(), fbs::PacketPayload::SubscribeTopicPacket));
  return fbb;
}

flatbuffers::FlatBufferBuilder BuildPublishMessagePacket(
    uint64_t request_id, std::string_view topic_name,
    std::string_view message_type, std::span<const uint8_t> payload) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto inner = fbs::CreatePublishMessagePacket(
      fbb, request_id, fbb.CreateString(topic_name.data(), topic_name.size()),
      fbb.CreateString(message_type.data(), message_type.size()),
      fbb.CreateVector(payload.data(), payload.size()));
  static_cast<void>(BuildBridgePacket(
      fbb, inner.Union(), fbs::PacketPayload::PublishMessagePacket));
  return fbb;
}

flatbuffers::FlatBufferBuilder BuildQueryPlayersPacket(uint64_t request_id) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto inner = fbs::CreateQueryPlayersPacket(fbb, request_id);
  static_cast<void>(BuildBridgePacket(fbb, inner.Union(),
                                      fbs::PacketPayload::QueryPlayersPacket));
  return fbb;
}

flatbuffers::FlatBufferBuilder BuildGraphSnapshotPacketForDispatch() {
  flatbuffers::FlatBufferBuilder fbb;
  const auto inner = fbs::CreateGraphSnapshotPacket(fbb);
  static_cast<void>(BuildBridgePacket(fbb, inner.Union(),
                                      fbs::PacketPayload::GraphSnapshotPacket));
  return fbb;
}

auto TryReceiveDatagram(asio::ip::udp::socket& receiver)
    -> std::optional<std::vector<uint8_t>> {
  using namespace std::chrono_literals;

  std::error_code ec;
  receiver.non_blocking(true, ec);
  if (ec) {
    return std::nullopt;
  }

  const auto deadline = std::chrono::steady_clock::now() + 250ms;
  while (std::chrono::steady_clock::now() < deadline) {
    std::array<uint8_t, 65535> buffer{};
    asio::ip::udp::endpoint from;
    std::error_code receive_ec;
    const size_t n =
        receiver.receive_from(asio::buffer(buffer), from, 0, receive_ec);
    if (!receive_ec) {
      return std::vector<uint8_t>(buffer.begin(), buffer.begin() + n);
    }
    if (receive_ec != asio::error::would_block &&
        receive_ec != asio::error::try_again) {
      return std::nullopt;
    }
    std::this_thread::sleep_for(1ms);
  }

  return std::nullopt;
}

const fbs::BridgePacket* ParseBridgePacket(const std::vector<uint8_t>& bytes) {
  return fbs::GetBridgePacket(bytes.data());
}

template <typename T>
auto DequeueOne(CommandQueue& queue) -> std::optional<T> {
  T value(std::pmr::get_default_resource());
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

template <>
auto DequeueOne<QueryPlayersCmd>(CommandQueue& queue)
    -> std::optional<QueryPlayersCmd> {
  QueryPlayersCmd value{};
  if (queue.TypedStorage<QueryPlayersCmd>().Dequeue(value)) {
    return value;
  }
  return std::nullopt;
}

}  // namespace

TEST_SUITE("bridge::network::command::handlers") {
  TEST_CASE("bridge::network::SendFbb") {
    flatbuffers::FlatBufferBuilder fbb;
    const auto inner = fbs::CreateQueryGraphPacket(fbb, 42U);
    const auto root = fbs::CreateBridgePacket(
        fbb, fbs::PacketPayload::QueryGraphPacket, inner.Union());
    fbb.Finish(root);

    asio::io_context io_ctx;
    asio::ip::udp::socket sender(io_ctx);
    sender.open(asio::ip::udp::v4());

    asio::ip::udp::socket receiver(io_ctx);
    receiver.open(asio::ip::udp::v4());
    receiver.bind(
        asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));

    const std::array<asio::ip::udp::endpoint, 1> clients{
        receiver.local_endpoint()};
    UdpTransport transport(sender, clients);

    SendFbb(transport, fbb);

    const auto datagram = TryReceiveDatagram(receiver);
    REQUIRE(datagram.has_value());
    const auto* packet = ParseBridgePacket(*datagram);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::QueryGraphPacket);
    REQUIRE(packet->payload_as_QueryGraphPacket() != nullptr);
    CHECK_EQ(packet->payload_as_QueryGraphPacket()->request_id(), 42U);
  }

  TEST_CASE("bridge::network::GraphHandler::Receive") {
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterIncomingQueueTypes(incoming);
    RegisterOutgoingQueueTypes(outgoing);

    GraphHandler handler = GraphHandler::From(incoming, outgoing);
    std::pmr::monotonic_buffer_resource arena;

    auto packet_fbb = BuildQueryGraphPacket(110U);
    const auto* packet = fbs::GetBridgePacket(packet_fbb.GetBufferPointer());

    handler.Receive(incoming, *packet, arena);

    const auto cmd = DequeueOne<QueryGraphCmd>(incoming);
    REQUIRE(cmd.has_value());
    CHECK_EQ(cmd->request_id, 110U);
    CHECK_FALSE(incoming.HasCommands<QueryGraphCmd>());
  }

  TEST_CASE("bridge::network::GraphHandler::DrainAndSend") {
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterIncomingQueueTypes(incoming);
    RegisterOutgoingQueueTypes(outgoing);

    GraphHandler handler = GraphHandler::From(incoming, outgoing);

    GraphSnapshotCmd first(std::pmr::get_default_resource());
    first.request_id = 1U;
    first.topics.emplace_back("/topic/one");
    first.services.emplace_back("/service/one");
    first.actions.emplace_back("/action/one");

    GraphSnapshotCmd second(std::pmr::get_default_resource());
    second.request_id = 2U;
    second.topics.emplace_back("/topic/two");
    second.services.emplace_back("/service/two");
    second.actions.emplace_back("/action/two");

    outgoing.Enqueue(std::move(first));
    outgoing.Enqueue(std::move(second));

    asio::io_context io_ctx;
    asio::ip::udp::socket sender(io_ctx);
    sender.open(asio::ip::udp::v4());
    asio::ip::udp::socket receiver(io_ctx);
    receiver.open(asio::ip::udp::v4());
    receiver.bind(
        asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    const std::array<asio::ip::udp::endpoint, 1> clients{
        receiver.local_endpoint()};
    UdpTransport transport(sender, clients);

    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndSend(outgoing, transport, fbb);

    const auto first_datagram = TryReceiveDatagram(receiver);
    const auto second_datagram = TryReceiveDatagram(receiver);
    REQUIRE(first_datagram.has_value());
    REQUIRE(second_datagram.has_value());

    const auto* first_packet = ParseBridgePacket(*first_datagram);
    const auto* second_packet = ParseBridgePacket(*second_datagram);

    REQUIRE(first_packet != nullptr);
    REQUIRE(second_packet != nullptr);

    CHECK_EQ(first_packet->payload_type(),
             fbs::PacketPayload::GraphSnapshotPacket);
    CHECK_EQ(second_packet->payload_type(),
             fbs::PacketPayload::GraphSnapshotPacket);

    const auto* first_payload = first_packet->payload_as_GraphSnapshotPacket();
    const auto* second_payload =
        second_packet->payload_as_GraphSnapshotPacket();
    REQUIRE(first_payload != nullptr);
    REQUIRE(second_payload != nullptr);

    CHECK_EQ(first_payload->request_id(), 1U);
    CHECK_EQ(second_payload->request_id(), 2U);
    CHECK_EQ(first_payload->topics()->Get(0)->string_view(), "/topic/one");
    CHECK_EQ(second_payload->topics()->Get(0)->string_view(), "/topic/two");
  }

  TEST_CASE("bridge::network::GraphHandler::From") {
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterIncomingQueueTypes(incoming);
    RegisterOutgoingQueueTypes(outgoing);

    GraphHandler handler = GraphHandler::From(incoming, outgoing);

    CHECK_NOTHROW(static_cast<void>(handler.in_producer));
    CHECK_NOTHROW(static_cast<void>(handler.out_consumer));
  }

  TEST_CASE("bridge::network::SubscribeTopicHandler::Receive") {
    CommandQueue incoming;
    RegisterIncomingQueueTypes(incoming);

    SubscribeTopicHandler handler = SubscribeTopicHandler::From(incoming);
    std::pmr::monotonic_buffer_resource arena;

    auto packet_fbb =
        BuildSubscribeTopicPacket(121U, "/topic/sub", "std_msgs/msg/String");
    const auto* packet = fbs::GetBridgePacket(packet_fbb.GetBufferPointer());

    handler.Receive(incoming, *packet, arena);

    const auto cmd = DequeueOne<SubscribeTopicCmd>(incoming);
    REQUIRE(cmd.has_value());
    CHECK_EQ(cmd->request_id, 121U);
    CHECK_EQ(cmd->topic_name, "/topic/sub");
    CHECK_EQ(cmd->message_type, "std_msgs/msg/String");
    CHECK_FALSE(incoming.HasCommands<SubscribeTopicCmd>());
  }

  TEST_CASE("bridge::network::SubscribeTopicHandler::From") {
    CommandQueue incoming;
    RegisterIncomingQueueTypes(incoming);

    SubscribeTopicHandler handler = SubscribeTopicHandler::From(incoming);

    CHECK_NOTHROW(static_cast<void>(handler.in_producer));
  }

  TEST_CASE("bridge::network::PublishMessageHandler::Receive") {
    CommandQueue incoming;
    RegisterIncomingQueueTypes(incoming);

    PublishMessageHandler handler = PublishMessageHandler::From(incoming);
    std::pmr::monotonic_buffer_resource arena;

    const std::array<uint8_t, 4> payload{9U, 8U, 7U, 6U};
    auto packet_fbb = BuildPublishMessagePacket(
        131U, "/topic/pub", "std_msgs/msg/UInt8MultiArray", payload);
    const auto* packet = fbs::GetBridgePacket(packet_fbb.GetBufferPointer());

    handler.Receive(incoming, *packet, arena);

    const auto cmd = DequeueOne<PublishMessageCmd>(incoming);
    REQUIRE(cmd.has_value());
    CHECK_EQ(cmd->request_id, 131U);
    CHECK_EQ(cmd->topic_name, "/topic/pub");
    CHECK_EQ(cmd->message_type, "std_msgs/msg/UInt8MultiArray");
    CHECK(std::ranges::equal(std::span(cmd->payload), payload));
    CHECK_FALSE(incoming.HasCommands<PublishMessageCmd>());
  }

  TEST_CASE("bridge::network::PublishMessageHandler::From") {
    CommandQueue incoming;
    RegisterIncomingQueueTypes(incoming);

    PublishMessageHandler handler = PublishMessageHandler::From(incoming);

    CHECK_NOTHROW(static_cast<void>(handler.in_producer));
  }

  TEST_CASE("bridge::network::PlayerListHandler::Receive") {
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterIncomingQueueTypes(incoming);
    RegisterOutgoingQueueTypes(outgoing);

    PlayerListHandler handler = PlayerListHandler::From(incoming, outgoing);
    std::pmr::monotonic_buffer_resource arena;

    auto packet_fbb = BuildQueryPlayersPacket(141U);
    const auto* packet = fbs::GetBridgePacket(packet_fbb.GetBufferPointer());

    handler.Receive(incoming, *packet, arena);

    const auto cmd = DequeueOne<QueryPlayersCmd>(incoming);
    REQUIRE(cmd.has_value());
    CHECK_EQ(cmd->request_id, 141U);
    CHECK_FALSE(incoming.HasCommands<QueryPlayersCmd>());
  }

  TEST_CASE("bridge::network::PlayerListHandler::DrainAndSend") {
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterIncomingQueueTypes(incoming);
    RegisterOutgoingQueueTypes(outgoing);

    PlayerListHandler handler = PlayerListHandler::From(incoming, outgoing);

    PlayerListCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 151U;
    cmd.players.emplace_back(std::pmr::get_default_resource());
    cmd.players[0].name = "Alex";
    cmd.players[0].x = 1.5F;
    cmd.players[0].y = 2.5F;
    cmd.players[0].z = 3.5F;

    outgoing.Enqueue(std::move(cmd));

    asio::io_context io_ctx;
    asio::ip::udp::socket sender(io_ctx);
    sender.open(asio::ip::udp::v4());
    asio::ip::udp::socket receiver(io_ctx);
    receiver.open(asio::ip::udp::v4());
    receiver.bind(
        asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    const std::array<asio::ip::udp::endpoint, 1> clients{
        receiver.local_endpoint()};
    UdpTransport transport(sender, clients);

    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndSend(outgoing, transport, fbb);

    const auto datagram = TryReceiveDatagram(receiver);
    REQUIRE(datagram.has_value());
    const auto* packet = ParseBridgePacket(*datagram);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::PlayerListPacket);

    const auto* payload_packet = packet->payload_as_PlayerListPacket();
    REQUIRE(payload_packet != nullptr);
    CHECK_EQ(payload_packet->request_id(), 151U);
    REQUIRE(payload_packet->players() != nullptr);
    CHECK_EQ(payload_packet->players()->size(), 1U);
    CHECK_EQ(payload_packet->players()->Get(0)->name()->string_view(), "Alex");
    CHECK_EQ(payload_packet->players()->Get(0)->x(), doctest::Approx(1.5F));
    CHECK_EQ(payload_packet->players()->Get(0)->y(), doctest::Approx(2.5F));
    CHECK_EQ(payload_packet->players()->Get(0)->z(), doctest::Approx(3.5F));
  }

  TEST_CASE("bridge::network::PlayerListHandler::From") {
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterIncomingQueueTypes(incoming);
    RegisterOutgoingQueueTypes(outgoing);

    PlayerListHandler handler = PlayerListHandler::From(incoming, outgoing);

    CHECK_NOTHROW(static_cast<void>(handler.in_producer));
    CHECK_NOTHROW(static_cast<void>(handler.out_consumer));
  }

  TEST_CASE("bridge::network::TopicPayloadHandler::DrainAndSend") {
    CommandQueue outgoing;
    RegisterOutgoingQueueTypes(outgoing);

    TopicPayloadHandler handler = TopicPayloadHandler::From(outgoing);

    TopicPayloadCmd cmd(std::pmr::get_default_resource());
    cmd.topic_name = "/topic/payload";
    cmd.message_type = "std_msgs/msg/String";
    cmd.payload.assign({1U, 3U, 5U, 7U});
    outgoing.Enqueue(std::move(cmd));

    asio::io_context io_ctx;
    asio::ip::udp::socket sender(io_ctx);
    sender.open(asio::ip::udp::v4());
    asio::ip::udp::socket receiver(io_ctx);
    receiver.open(asio::ip::udp::v4());
    receiver.bind(
        asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    const std::array<asio::ip::udp::endpoint, 1> clients{
        receiver.local_endpoint()};
    UdpTransport transport(sender, clients);

    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndSend(outgoing, transport, fbb);

    const auto datagram = TryReceiveDatagram(receiver);
    REQUIRE(datagram.has_value());
    const auto* packet = ParseBridgePacket(*datagram);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(), fbs::PacketPayload::TopicPayloadPacket);

    const auto* payload_packet = packet->payload_as_TopicPayloadPacket();
    REQUIRE(payload_packet != nullptr);
    CHECK_EQ(payload_packet->topic_name()->string_view(), "/topic/payload");
    CHECK_EQ(payload_packet->message_type()->string_view(),
             "std_msgs/msg/String");
    CHECK_EQ(payload_packet->payload()->size(), 4U);
    CHECK_EQ(payload_packet->payload()->Get(0), 1U);
    CHECK_EQ(payload_packet->payload()->Get(1), 3U);
    CHECK_EQ(payload_packet->payload()->Get(2), 5U);
    CHECK_EQ(payload_packet->payload()->Get(3), 7U);

    const auto no_more_datagrams = TryReceiveDatagram(receiver);
    CHECK_FALSE(no_more_datagrams.has_value());
  }

  TEST_CASE("bridge::network::TopicPayloadHandler::From") {
    CommandQueue outgoing;
    RegisterOutgoingQueueTypes(outgoing);

    TopicPayloadHandler handler = TopicPayloadHandler::From(outgoing);

    CHECK_NOTHROW(static_cast<void>(handler.out_consumer));
  }

  TEST_CASE("bridge::network::DispatchReceive") {
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterIncomingQueueTypes(incoming);
    RegisterOutgoingQueueTypes(outgoing);

    CommandHandlerRegistry registry;
    registry.AddHandler(GraphHandler::From(incoming, outgoing));
    registry.AddHandler(SubscribeTopicHandler::From(incoming));
    registry.AddHandler(PublishMessageHandler::From(incoming));
    registry.AddHandler(PlayerListHandler::From(incoming, outgoing));

    std::pmr::monotonic_buffer_resource arena;

    auto query_graph_fbb = BuildQueryGraphPacket(201U);
    auto subscribe_fbb = BuildSubscribeTopicPacket(202U, "/dispatch/topic",
                                                   "std_msgs/msg/String");
    const std::array<uint8_t, 3> payload{4U, 2U, 0U};
    auto publish_fbb = BuildPublishMessagePacket(
        203U, "/dispatch/publish", "std_msgs/msg/UInt8MultiArray", payload);
    auto players_fbb = BuildQueryPlayersPacket(204U);
    auto unknown_fbb = BuildGraphSnapshotPacketForDispatch();

    const auto* query_graph_packet =
        fbs::GetBridgePacket(query_graph_fbb.GetBufferPointer());
    const auto* subscribe_packet =
        fbs::GetBridgePacket(subscribe_fbb.GetBufferPointer());
    const auto* publish_packet =
        fbs::GetBridgePacket(publish_fbb.GetBufferPointer());
    const auto* players_packet =
        fbs::GetBridgePacket(players_fbb.GetBufferPointer());
    const auto* unknown_packet =
        fbs::GetBridgePacket(unknown_fbb.GetBufferPointer());

    DispatchReceive(registry, incoming, *query_graph_packet, arena);
    DispatchReceive(registry, incoming, *subscribe_packet, arena);
    DispatchReceive(registry, incoming, *publish_packet, arena);
    DispatchReceive(registry, incoming, *players_packet, arena);
    DispatchReceive(registry, incoming, *unknown_packet, arena);

    const auto query_graph_cmd = DequeueOne<QueryGraphCmd>(incoming);
    REQUIRE(query_graph_cmd.has_value());
    CHECK_EQ(query_graph_cmd->request_id, 201U);

    const auto subscribe_cmd = DequeueOne<SubscribeTopicCmd>(incoming);
    REQUIRE(subscribe_cmd.has_value());
    CHECK_EQ(subscribe_cmd->request_id, 202U);
    CHECK_EQ(subscribe_cmd->topic_name, "/dispatch/topic");

    const auto publish_cmd = DequeueOne<PublishMessageCmd>(incoming);
    REQUIRE(publish_cmd.has_value());
    CHECK_EQ(publish_cmd->request_id, 203U);
    CHECK(std::ranges::equal(std::span(publish_cmd->payload), payload));

    const auto query_players_cmd = DequeueOne<QueryPlayersCmd>(incoming);
    REQUIRE(query_players_cmd.has_value());
    CHECK_EQ(query_players_cmd->request_id, 204U);

    CHECK_FALSE(incoming.HasCommands<QueryGraphCmd>());
    CHECK_FALSE(incoming.HasCommands<SubscribeTopicCmd>());
    CHECK_FALSE(incoming.HasCommands<PublishMessageCmd>());
    CHECK_FALSE(incoming.HasCommands<QueryPlayersCmd>());
  }

  TEST_CASE("bridge::network::DrainAndSendHandlerTypes") {
    CHECK((std::same_as<
           DrainAndSendHandlerTypes,
           std::tuple<GraphHandler, PlayerListHandler, TopicPayloadHandler>>));
  }
}
