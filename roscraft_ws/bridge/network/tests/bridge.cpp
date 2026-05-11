#include <doctest/doctest.h>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/app/config.hpp>
#include <roscraft/bridge/bridge.hpp>
#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/network/bridge.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/udp.hpp>

#include <flatbuffers/flatbuffers.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory_resource>
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

[[nodiscard]] auto BuildServiceCallDatagram(uint64_t request_id)
    -> std::vector<uint8_t> {
  flatbuffers::FlatBufferBuilder fbb;
  const auto service_name = fbb.CreateString("/service");
  const auto service_type = fbb.CreateString("unsupported/srv/Type");
  const std::array<uint8_t, 3> payload{9U, 8U, 7U};
  const auto payload_offset = fbb.CreateVector(payload.data(), payload.size());
  const auto inner =
      fbs::CreateServiceCallPacket(fbb, request_id, service_name, service_type,
                                   payload_offset, 0.25, 0U, 0.0);
  const auto packet = fbs::CreateBridgePacket(
      fbb, fbs::PacketPayload::ServiceCallPacket, inner.Union());
  fbs::FinishBridgePacketBuffer(fbb, packet);

  const auto* begin = fbb.GetBufferPointer();
  const auto* end = begin + fbb.GetSize();
  return {begin, end};
}

[[nodiscard]] auto BuildParamListDatagram(uint64_t request_id)
    -> std::vector<uint8_t> {
  flatbuffers::FlatBufferBuilder fbb;
  std::vector<flatbuffers::Offset<flatbuffers::String>> prefixes;
  prefixes.push_back(fbb.CreateString("/robot"));
  const auto inner = fbs::CreateParamListPacketDirect(
      fbb, request_id, "", &prefixes, 2U, true, "^foo", 0.25);
  const auto packet = fbs::CreateBridgePacket(
      fbb, fbs::PacketPayload::ParamListPacket, inner.Union());
  fbs::FinishBridgePacketBuffer(fbb, packet);

  const auto* begin = fbb.GetBufferPointer();
  const auto* end = begin + fbb.GetSize();
  return {begin, end};
}

[[nodiscard]] auto BuildActionSendGoalDatagram(uint64_t request_id)
    -> std::vector<uint8_t> {
  flatbuffers::FlatBufferBuilder fbb;
  const auto action_name = fbb.CreateString("/demo/action");
  const auto action_type = fbb.CreateString("unsupported/action/Type");
  const std::array<uint8_t, 3> goal_payload{9U, 8U, 7U};
  const auto goal_payload_offset =
      fbb.CreateVector(goal_payload.data(), goal_payload.size());
  const auto inner =
      fbs::CreateActionSendGoalPacket(fbb, request_id, action_name, action_type,
                                      goal_payload_offset, true, 0.25);
  const auto packet = fbs::CreateBridgePacket(
      fbb, fbs::PacketPayload::ActionSendGoalPacket, inner.Union());
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

template <typename Handler>
void DrainReceivedDatagrams(asio::ip::udp::socket& socket, Handler&& handler) {
  std::error_code ec;
  socket.non_blocking(true, ec);
  if (ec) {
    return;
  }

  while (true) {
    std::array<uint8_t, 65535> buffer{};
    asio::ip::udp::endpoint from;
    std::error_code receive_ec;
    const size_t n =
        socket.receive_from(asio::buffer(buffer), from, 0, receive_ec);
    if (!receive_ec) {
      handler(std::span<const uint8_t>(buffer.data(), n));
      continue;
    }

    if (receive_ec == asio::error::would_block ||
        receive_ec == asio::error::try_again) {
      return;
    }

    return;
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

      std::optional<std::vector<uint8_t>> response;

      TickUntil(app, bridge, [&] {
        if (app.IncomingQueue().HasCommands<QueryGraphCmd>()) {
          return true;
        }
        if (!response.has_value()) {
          response = TryReceiveDatagram(sender);
        }
        return response.has_value();
      });

      CHECK_EQ(bridge.ClientCount(), 1U);

      if (app.IncomingQueue().HasCommands<QueryGraphCmd>()) {
        QueryGraphCmd cmd{};
        CHECK(app.IncomingQueue().TypedStorage<QueryGraphCmd>().Dequeue(cmd));
        CHECK_EQ(cmd.request_id, 9001U);
      } else {
        REQUIRE(response.has_value());

        const auto* packet = fbs::GetBridgePacket(response->data());
        REQUIRE(packet != nullptr);
        CHECK_EQ(packet->payload_type(),
                 fbs::PacketPayload::GraphSnapshotPacket);
        REQUIRE(packet->payload_as_GraphSnapshotPacket() != nullptr);
        CHECK_EQ(packet->payload_as_GraphSnapshotPacket()->request_id(), 9001U);
      }
    }

    SUBCASE("Tick dispatches service, param, and action packets") {
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

      const auto service_datagram = BuildServiceCallDatagram(9101U);
      const auto param_datagram = BuildParamListDatagram(9102U);
      const auto action_datagram = BuildActionSendGoalDatagram(9103U);
      SendDatagram(sender, port, service_datagram);
      SendDatagram(sender, port, param_datagram);
      SendDatagram(sender, port, action_datagram);

      bool saw_service_cmd = false;
      bool saw_param_cmd = false;
      bool saw_action_cmd = false;

      TickUntil(app, bridge, [&] {
        if (!saw_service_cmd &&
            app.IncomingQueue().HasCommands<ServiceCallCmd>()) {
          saw_service_cmd = true;
        }
        if (!saw_param_cmd && app.IncomingQueue().HasCommands<ParamListCmd>()) {
          saw_param_cmd = true;
        }
        if (!saw_action_cmd &&
            app.IncomingQueue().HasCommands<ActionSendGoalCmd>()) {
          saw_action_cmd = true;
        }
        if (!saw_action_cmd &&
            app.OutgoingQueue().HasCommands<ActionResultCmd>()) {
          saw_action_cmd = true;
        }
        return saw_service_cmd && saw_param_cmd && saw_action_cmd;
      });

      CHECK(saw_service_cmd);
      CHECK(saw_param_cmd);
      CHECK(saw_action_cmd);
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
      CHECK_FALSE(app.IncomingQueue().HasCommands<TopicSubscribeCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<TopicPublishMessageCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<ServiceCallCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<ParamListCmd>());
      CHECK_FALSE(app.IncomingQueue().HasCommands<ActionSendGoalCmd>());
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
      {
        auto& t =
            snapshot.topics.emplace_back(std::pmr::get_default_resource());
        t.name = "/graph/topic";
        t.type = "std_msgs/msg/String";
      }
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

    SUBCASE("Tick drains service, param, and action response packets") {
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

      ServiceCallResponseCmd service_cmd(std::pmr::get_default_resource());
      service_cmd.request_id = 301U;
      service_cmd.service_name = "/service";
      service_cmd.service_type = "std_srvs/srv/Trigger";
      service_cmd.success = true;
      service_cmd.response_payload = {1U, 2U};
      service_cmd.result_text = "ok";
      app.OutgoingQueue().Enqueue(std::move(service_cmd));

      ParamListResponseCmd param_cmd(std::pmr::get_default_resource());
      param_cmd.request_id = 302U;
      param_cmd.node_name = "/node";
      param_cmd.names.emplace_back("foo");
      param_cmd.prefixes.emplace_back("/robot");
      param_cmd.types.emplace_back("integer");
      app.OutgoingQueue().Enqueue(std::move(param_cmd));

      ActionResultCmd action_cmd(std::pmr::get_default_resource());
      action_cmd.request_id = 303U;
      action_cmd.action_name = "/demo/action";
      action_cmd.action_type = "example_interfaces/action/Fibonacci";
      action_cmd.success = true;
      action_cmd.result_payload = {9U, 8U, 7U};
      action_cmd.result_text = "goal finished";
      app.OutgoingQueue().Enqueue(std::move(action_cmd));

      bool saw_service = false;
      bool saw_param = false;
      bool saw_action = false;

      TickUntil(app, bridge, [&] {
        DrainReceivedDatagrams(client, [&](std::span<const uint8_t> bytes) {
          const auto* packet = fbs::GetBridgePacket(bytes.data());
          if (packet == nullptr) {
            return;
          }

          switch (packet->payload_type()) {
            case fbs::PacketPayload::ServiceCallResponsePacket:
              saw_service = true;
              CHECK_EQ(
                  packet->payload_as_ServiceCallResponsePacket()->request_id(),
                  301U);
              return;
            case fbs::PacketPayload::ParamListResponsePacket:
              saw_param = true;
              CHECK_EQ(
                  packet->payload_as_ParamListResponsePacket()->request_id(),
                  302U);
              return;
            case fbs::PacketPayload::ActionResultPacket:
              saw_action = true;
              CHECK_EQ(packet->payload_as_ActionResultPacket()->request_id(),
                       303U);
              return;
            default:
              return;
          }
        });

        return saw_service && saw_param && saw_action;
      });

      CHECK(saw_service);
      CHECK(saw_param);
      CHECK(saw_action);
      CHECK_EQ(app.OutgoingQueue().CommandCount<ServiceCallResponseCmd>(), 0U);
      CHECK_EQ(app.OutgoingQueue().CommandCount<ParamListResponseCmd>(), 0U);
      CHECK_EQ(app.OutgoingQueue().CommandCount<ActionResultCmd>(), 0U);
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
