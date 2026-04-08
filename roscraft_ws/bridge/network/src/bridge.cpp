#include <pch.hpp>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/network/bridge.hpp>
#include <roscraft/bridge/network/command/handler_registry.hpp>
#include <roscraft/bridge/network/command/handlers.hpp>
#include <roscraft/bridge/network/config.hpp>
#include <roscraft/bridge/network/transport.hpp>
#include <roscraft/generated/bridge_packets_generated.hpp>

#include <argparse/argparse.hpp>

#include <glaze/toml.hpp>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/ip/udp.hpp>
#include <asio/redirect_error.hpp>
#include <asio/use_awaitable.hpp>

#include <flatbuffers/flatbuffers.h>

#include <rclcpp/logging.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory_resource>
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace roscraft::bridge::network {

void NetworkBridge::ParseArgs(int argc, char* argv[]) {
  if (argc < 2) [[unlikely]] {
    return;
  }

  argparse::ArgumentParser parser("roscraft_bridge_network");
  parser.add_argument("-h", "--host")
      .default_value(IpAddress("127.0.0.1"))
      .help("Host ip address");
  parser.add_argument("-p", "--port")
      .default_value(uint16_t{7401})
      .scan<'u', uint16_t>()
      .help("Port number");
  parser.add_argument("--allow-multiple-connections")
      .flag()
      .help("Allow multiple client connections");

  try {
    parser.parse_args(argc, argv);
    config_.host = parser.get<std::string>("--host");
    config_.port = parser.get<uint16_t>("--port");
    config_.allow_multiple_connections =
        parser.get<bool>("--allow-multiple-connections");
  } catch (const std::exception& e) {
    RCLCPP_WARN(rclcpp::get_logger("NetworkBridge"),
                "Failed to parse command line arguments: %s", e.what());
  }
}

void NetworkBridge::Init(App& app) {
  app_.emplace(app);
  status_.store(BridgeStatus::kInitializing, std::memory_order_release);

  InitCommandHandlerRegistry();

  InitAsio();

  status_.store(BridgeStatus::kReady, std::memory_order_release);
  RCLCPP_INFO(rclcpp::get_logger("NetworkBridge"), "Listening on udp://%s:%u",
              config_.host.CStr(), config_.port);
}

void NetworkBridge::Destroy(App& /*app*/) {
  status_.store(BridgeStatus::kShuttingDown, std::memory_order_release);
  DestroyAsio();

  {
    std::scoped_lock lock(clients_mutex_);
    clients_.clear();
  }

  app_.reset();

  status_.store(BridgeStatus::kUninitialized, std::memory_order_release);
}

void NetworkBridge::Tick(App& /*app*/) {
  if (Status() != BridgeStatus::kReady) [[unlikely]] {
    return;
  }

  io_ctx_.restart();
  io_ctx_.poll();

  DrainAndSendAll();
}

void NetworkBridge::InitCommandHandlerRegistry() {
  ROSCRAFT_ASSERT(app_.has_value(), "App reference is not set!");
  auto& app = app_->get();
  auto& in = app.IncomingQueue();
  auto& out = app.OutgoingQueue();

  registry_.AddHandler(GraphHandler::From(in, out));
  registry_.AddHandler(SubscribeTopicHandler::From(in));
  registry_.AddHandler(PublishMessageHandler::From(in));
  registry_.AddHandler(PlayerListHandler::From(in, out));
  registry_.AddHandler(TopicPayloadHandler::From(out));
}

void NetworkBridge::InitAsio() {
  io_ctx_.restart();

  const asio::ip::udp::endpoint ep(asio::ip::make_address(config_.host),
                                   config_.port);
  socket_.open(ep.protocol());
  socket_.set_option(asio::socket_base::reuse_address(true));
  socket_.bind(ep);

  work_guard_.emplace(asio::make_work_guard(io_ctx_));

  asio::co_spawn(io_ctx_, ReceiveLoop(), asio::detached);
}

void NetworkBridge::DestroyAsio() {
  if (work_guard_.has_value()) {
    work_guard_->reset();
    work_guard_.reset();
  }
  io_ctx_.stop();

  std::error_code ec;
  socket_.close(ec);
}

auto NetworkBridge::ReceiveLoop() -> asio::awaitable<void> {
  ROSCRAFT_ASSERT(app_.has_value(), "App reference is not set!");
  auto& app = app_->get();

  while (!app.IsShutdownRequested()) {
    std::error_code ec;
    asio::ip::udp::endpoint sender;
    const size_t n = co_await socket_.async_receive_from(
        asio::buffer(recv_buf_), sender,
        asio::redirect_error(asio::use_awaitable, ec));

    if (ec == asio::error::operation_aborted) [[unlikely]] {
      co_return;
    }
    if (ec) [[unlikely]] {
      RCLCPP_WARN(rclcpp::get_logger("NetworkBridge"), "Receive error: %s!",
                  ec.message().c_str());
      continue;
    }

    // Manage client connections with minimal locking
    if (config_.allow_multiple_connections) {
      // Multi-client mode: use read-modify-write pattern
      std::shared_lock lock(clients_mutex_);
      if (clients_.contains(sender)) {
        // Already registered, skip write lock
        lock.unlock();
      } else {
        // Need to add new client - upgrade to unique lock
        lock.unlock();
        AddClient(sender);
      }

    } else {
      // Single-client mode: only accept from the first connected client
      // Fast path: check with shared lock
      std::shared_lock lock(clients_mutex_);
      if (clients_.empty()) {
        // Need to register first client
        lock.unlock();
        AddClient(sender);
      } else {
        // Check if sender is the registered client
        const auto& registered = *clients_.begin();
        lock.unlock();
        if (sender != registered) {
          // Ignore packets from unknown clients in single-client mode
          continue;
        }
      }
    }

    HandleDatagram({recv_buf_.data(), n}, sender, app.PendingFrameAllocator());
  }
}

void NetworkBridge::DrainAndSendAll() {
  ROSCRAFT_ASSERT(app_.has_value(), "App reference is not set!");
  auto& app = app_->get();

  // Thread-local FlatBufferBuilder avoids per-message heap allocation.
  // Safe here because DrainAndSendAll always runs on the single ASIO thread.
  thread_local flatbuffers::FlatBufferBuilder fbb(4096);

  // Build a local snapshot of clients under the read lock, then release it
  // before doing any I/O. Using a local (not thread_local) set so that each
  // call sees exactly the clients registered at this moment, with no
  // cross-call bleed-over (which thread_local would cause on Reload()).
  const auto clients_snapshot = BuildClientSnapshot();

  if (clients_snapshot.empty()) {
    return;
  }

  UdpTransport transport(socket_,
                         std::span<const asio::ip::udp::endpoint>(
                             clients_snapshot.data(), clients_snapshot.size()));
  registry_.DrainAndSendAll<DrainAndSendHandlerTypes>(app.OutgoingQueue(),
                                                      transport, fbb);
}

auto NetworkBridge::BuildClientSnapshot() const
    -> std::vector<asio::ip::udp::endpoint> {
  std::shared_lock lock(clients_mutex_);
  return {clients_.begin(), clients_.end()};
}

void NetworkBridge::HandleDatagram(
    std::span<const uint8_t> data, const asio::ip::udp::endpoint& from,
    std::pmr::memory_resource& pending_allocator) {
  ROSCRAFT_ASSERT(app_.has_value(), "App reference is not set!");
  auto& app = app_->get();

  flatbuffers::Verifier verifier(data.data(), data.size());
  if (!fbs::VerifyBridgePacketBuffer(verifier)) [[unlikely]] {
    RCLCPP_WARN(rclcpp::get_logger("NetworkBridge"),
                "Dropping malformed datagram (%zu B) from %s!", data.size(),
                from.address().to_string().c_str());
    return;
  }

  // Dispatch to appropriate handler using arena for transient allocations
  DispatchReceive(registry_, app.IncomingQueue(),
                  *fbs::GetBridgePacket(data.data()), pending_allocator);
}

}  // namespace roscraft::bridge::network
