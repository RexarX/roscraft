#include <doctest/doctest.h>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/app/config.hpp>
#include <roscraft/bridge/bridge.hpp>
#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/network/bridge.hpp>
#include <roscraft/generated/bridge_packets_generated.hpp>

#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/udp.hpp>

#include <flatbuffers/flatbuffers.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <thread>
#include <vector>

using namespace roscraft::bridge;
using namespace roscraft::bridge::network;

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

[[nodiscard]] BridgeConfig MakeBridgeConfig(
    bool allow_multiple_connections = false) {
  BridgeConfig config;
  config.host = "127.0.0.1";
  config.port = 0;
  config.allow_multiple_connections = allow_multiple_connections;
  return config;
}

[[nodiscard]] BridgeConfig MakeBridgeConfig(
    uint16_t port, bool allow_multiple_connections = false) {
  BridgeConfig config = MakeBridgeConfig(allow_multiple_connections);
  config.port = port;
  return config;
}

[[nodiscard]] uint16_t ReserveUdpPort() {
  asio::io_context io_ctx;
  asio::ip::udp::socket socket(io_ctx);
  socket.open(asio::ip::udp::v4());
  socket.bind(asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
  return socket.local_endpoint().port();
}

void SendDatagram(asio::ip::udp::socket& sender, uint16_t server_port,
                  std::span<const uint8_t> bytes) {
  sender.send_to(asio::buffer(bytes.data(), bytes.size()),
                 asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"),
                                         server_port));
}

[[nodiscard]] auto BuildQueryGraphDatagram(uint64_t request_id)
    -> std::vector<uint8_t> {
  flatbuffers::FlatBufferBuilder fbb;
  const auto payload = fbs::CreateQueryGraphPacket(fbb, request_id);
  const auto packet = fbs::CreateBridgePacket(
      fbb, fbs::PacketPayload::QueryGraphPacket, payload.Union());
  fbs::FinishBridgePacketBuffer(fbb, packet);

  const auto* begin = fbb.GetBufferPointer();
  const auto* end = begin + fbb.GetSize();
  return {begin, end};
}

[[nodiscard]] auto TryReceiveDatagram(asio::ip::udp::socket& socket)
    -> std::optional<std::vector<uint8_t>> {
  using namespace std::chrono_literals;

  std::error_code ec;
  socket.non_blocking(true, ec);
  if (ec) {
    return std::nullopt;
  }

  const auto deadline = std::chrono::steady_clock::now() + 250ms;
  while (std::chrono::steady_clock::now() < deadline) {
    std::array<uint8_t, 65535> buffer{};
    asio::ip::udp::endpoint from;
    std::error_code receive_ec;
    const size_t n =
        socket.receive_from(asio::buffer(buffer), from, 0, receive_ec);
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

template <typename Pred>
void TickUntil(App& app, NetworkBridge& bridge, Pred&& pred) {
  using namespace std::chrono_literals;

  for (int i = 0; i < 64 && !pred(); ++i) {
    bridge.Tick(app);
    std::this_thread::sleep_for(1ms);
  }
}

}  // namespace

TEST_SUITE("bridge::network::NetworkBridge") {
  TEST_CASE("bridge::network::NetworkBridge::ctor") {
    SUBCASE("Default ctor uses default bridge configuration") {
      const NetworkBridge bridge;

      CHECK_EQ(bridge.Status(), BridgeStatus::kUninitialized);
      CHECK_EQ(bridge.ClientCount(), 0U);
      CHECK_EQ(bridge.Config().host.View(), BridgeConfig::kDefaultHost.View());
      CHECK_EQ(bridge.Config().port, BridgeConfig::kDefaultPort);
      CHECK_FALSE(bridge.Config().allow_multiple_connections);
    }

    SUBCASE("Ctor stores caller-provided configuration") {
      const BridgeConfig config = MakeBridgeConfig(true);
      const NetworkBridge bridge(config);

      CHECK_EQ(bridge.Config().host.View(), config.host.View());
      CHECK_EQ(bridge.Config().port, config.port);
      CHECK(bridge.Config().allow_multiple_connections);
    }
  }

  TEST_CASE("bridge::network::NetworkBridge::~NetworkBridge") {
    ScopedAppGuard app_guard;
    auto& app = App::Instance();
    app.Init(AppConfig::From<NetworkBridge>(MakeBridgeConfig()));

    auto& bridge = app.GetBridge<NetworkBridge>();
    CHECK_EQ(bridge.Status(), BridgeStatus::kReady);

    app.Shutdown();
    CHECK_EQ(app.State(), AppState::kUninitialized);
  }

  TEST_CASE("bridge::network::NetworkBridge::Init") {
    ScopedAppGuard app_guard;
    auto& app = App::Instance();
    app.Init(AppConfig::From<NetworkBridge>(MakeBridgeConfig()));

    auto& bridge = app.GetBridge<NetworkBridge>();

    CHECK_EQ(bridge.Status(), BridgeStatus::kReady);
    CHECK_EQ(bridge.ClientCount(), 0U);

    CHECK(app.IncomingQueue().IsRegistered<QueryGraphCmd>());
    CHECK(app.IncomingQueue().IsRegistered<SubscribeTopicCmd>());
    CHECK(app.IncomingQueue().IsRegistered<PublishMessageCmd>());
    CHECK(app.IncomingQueue().IsRegistered<QueryPlayersCmd>());

    CHECK(app.OutgoingQueue().IsRegistered<GraphSnapshotCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<PlayerListCmd>());
    CHECK(app.OutgoingQueue().IsRegistered<TopicPayloadCmd>());
  }

  TEST_CASE("bridge::network::NetworkBridge::Destroy") {
    ScopedAppGuard app_guard;
    auto& app = App::Instance();
    app.Init(AppConfig::From<NetworkBridge>(MakeBridgeConfig()));

    auto& bridge = app.GetBridge<NetworkBridge>();
    bridge.Destroy(app);

    CHECK_EQ(bridge.Status(), BridgeStatus::kUninitialized);
    CHECK_EQ(bridge.ClientCount(), 0U);
  }

  TEST_CASE("bridge::network::NetworkBridge::Reload") {
    ScopedAppGuard app_guard;
    auto& app = App::Instance();
    app.Init(AppConfig::From<NetworkBridge>(MakeBridgeConfig(true)));

    auto& bridge = app.GetBridge<NetworkBridge>();
    const Bridge* before = &bridge;
    bridge.Reload(app);
    const Bridge* after = &app.GetBridge<NetworkBridge>();

    CHECK_EQ(bridge.Status(), BridgeStatus::kReady);
    CHECK(bridge.Config().allow_multiple_connections);
    CHECK_EQ(before, after);
  }

  TEST_CASE("bridge::network::NetworkBridge::Tick") {
    SUBCASE("Tick keeps bridge ready with no traffic") {
      ScopedAppGuard app_guard;
      auto& app = App::Instance();
      app.Init(AppConfig::From<NetworkBridge>(MakeBridgeConfig()));

      auto& bridge = app.GetBridge<NetworkBridge>();
      bridge.Tick(app);

      CHECK_EQ(bridge.Status(), BridgeStatus::kReady);
    }

    SUBCASE("Tick processes valid datagrams and dispatches commands") {
      ScopedAppGuard app_guard;
      auto& app = App::Instance();
      const uint16_t port = ReserveUdpPort();
      app.Init(AppConfig::From<NetworkBridge>(MakeBridgeConfig(port)));

      auto& bridge = app.GetBridge<NetworkBridge>();

      asio::io_context io_ctx;
      asio::ip::udp::socket sender(io_ctx);
      sender.open(asio::ip::udp::v4());
      sender.bind(
          asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));

      const auto datagram = BuildQueryGraphDatagram(9001U);
      SendDatagram(sender, port, datagram);

      TickUntil(app, bridge, [&] {
        return app.IncomingQueue().HasCommands<QueryGraphCmd>();
      });

      CHECK_EQ(bridge.ClientCount(), 1U);
      CHECK(app.IncomingQueue().HasCommands<QueryGraphCmd>());

      QueryGraphCmd cmd{};
      CHECK(app.IncomingQueue().TypedStorage<QueryGraphCmd>().Dequeue(cmd));
      CHECK_EQ(cmd.request_id, 9001U);
    }

    SUBCASE("Tick drops malformed datagrams without dispatching commands") {
      ScopedAppGuard app_guard;
      auto& app = App::Instance();
      const uint16_t port = ReserveUdpPort();
      app.Init(AppConfig::From<NetworkBridge>(MakeBridgeConfig(port)));

      auto& bridge = app.GetBridge<NetworkBridge>();

      asio::io_context io_ctx;
      asio::ip::udp::socket sender(io_ctx);
      sender.open(asio::ip::udp::v4());
      sender.bind(
          asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));

      const std::array<uint8_t, 3> malformed{1U, 2U, 3U};
      SendDatagram(sender, port, malformed);

      TickUntil(app, bridge, [&] { return bridge.ClientCount() == 1U; });

      CHECK_FALSE(app.IncomingQueue().HasCommands<QueryGraphCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<SubscribeTopicCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<PublishMessageCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<QueryPlayersCmd>());
    }

    SUBCASE("Tick drains outgoing queue and sends packets to clients") {
      ScopedAppGuard app_guard;
      auto& app = App::Instance();
      const uint16_t port = ReserveUdpPort();
      app.Init(AppConfig::From<NetworkBridge>(MakeBridgeConfig(port)));

      auto& bridge = app.GetBridge<NetworkBridge>();

      asio::io_context io_ctx;
      asio::ip::udp::socket client(io_ctx);
      client.open(asio::ip::udp::v4());
      client.bind(
          asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));

      const std::array<uint8_t, 1> registration_ping{0U};
      SendDatagram(client, port, registration_ping);
      TickUntil(app, bridge, [&] { return bridge.ClientCount() == 1U; });

      GraphSnapshotCmd snapshot(std::pmr::get_default_resource());
      snapshot.request_id = 77U;
      snapshot.topics.emplace_back("/graph/topic");
      app.OutgoingQueue().Enqueue(std::move(snapshot));

      bridge.Tick(app);

      const auto bytes = TryReceiveDatagram(client);
      REQUIRE(bytes.has_value());

      const auto* packet = fbs::GetBridgePacket(bytes->data());
      REQUIRE(packet != nullptr);
      CHECK_EQ(packet->payload_type(), fbs::PacketPayload::GraphSnapshotPacket);
      REQUIRE(packet->payload_as_GraphSnapshotPacket() != nullptr);
      CHECK_EQ(packet->payload_as_GraphSnapshotPacket()->request_id(), 77U);
    }
  }

  TEST_CASE("bridge::network::NetworkBridge::ClientCount") {
    SUBCASE("ClientCount starts at zero") {
      const NetworkBridge bridge;

      CHECK_EQ(bridge.ClientCount(), 0U);
    }

    SUBCASE("Single-connection mode keeps one client") {
      ScopedAppGuard app_guard;
      auto& app = App::Instance();
      const uint16_t port = ReserveUdpPort();
      app.Init(AppConfig::From<NetworkBridge>(MakeBridgeConfig(port, false)));

      auto& bridge = app.GetBridge<NetworkBridge>();

      asio::io_context io_ctx;
      asio::ip::udp::socket first(io_ctx);
      asio::ip::udp::socket second(io_ctx);
      first.open(asio::ip::udp::v4());
      second.open(asio::ip::udp::v4());
      first.bind(
          asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
      second.bind(
          asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));

      const std::array<uint8_t, 2> malformed{1U, 2U};
      SendDatagram(first, port, malformed);
      SendDatagram(second, port, malformed);

      TickUntil(app, bridge, [&] { return bridge.ClientCount() == 1U; });

      CHECK_EQ(bridge.ClientCount(), 1U);
    }

    SUBCASE("Multi-connection mode tracks multiple clients") {
      ScopedAppGuard app_guard;
      auto& app = App::Instance();
      const uint16_t port = ReserveUdpPort();
      app.Init(AppConfig::From<NetworkBridge>(MakeBridgeConfig(port, true)));

      auto& bridge = app.GetBridge<NetworkBridge>();

      asio::io_context io_ctx;
      asio::ip::udp::socket first(io_ctx);
      asio::ip::udp::socket second(io_ctx);
      first.open(asio::ip::udp::v4());
      second.open(asio::ip::udp::v4());
      first.bind(
          asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
      second.bind(
          asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));

      const std::array<uint8_t, 2> malformed{1U, 2U};
      SendDatagram(first, port, malformed);
      SendDatagram(second, port, malformed);

      TickUntil(app, bridge, [&] { return bridge.ClientCount() == 2U; });

      CHECK_EQ(bridge.ClientCount(), 2U);
    }
  }

  TEST_CASE("bridge::network::NetworkBridge::Status") {
    const NetworkBridge bridge;

    CHECK_EQ(bridge.Status(), BridgeStatus::kUninitialized);
  }

  TEST_CASE("bridge::network::NetworkBridge::Config") {
    const BridgeConfig config = MakeBridgeConfig(true);
    const NetworkBridge bridge(config);

    CHECK_EQ(bridge.Config().host.View(), config.host.View());
    CHECK_EQ(bridge.Config().port, config.port);
    CHECK(bridge.Config().allow_multiple_connections);
  }
}
