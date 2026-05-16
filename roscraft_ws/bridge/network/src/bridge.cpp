#include <pch.hpp>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/handlers.hpp>
#include <roscraft/bridge/command/types/topic.hpp>
#include <roscraft/bridge/network/bridge.hpp>
#include <roscraft/bridge/network/command/udp_sink.hpp>
#include <roscraft/bridge/network/config.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/ip/udp.hpp>
#include <asio/redirect_error.hpp>
#include <asio/use_awaitable.hpp>

#include <flatbuffers/flatbuffers.h>

#include <rclcpp/logging.hpp>

#include <atomic>
#include <cstddef>
#include <format>
#include <memory_resource>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

[[nodiscard]] static std::string EndpointToString(
    const asio::ip::udp::endpoint& ep) {
  return std::format("{}:{}", ep.address().to_string(), ep.port());
}

namespace roscraft::bridge::network {

void NetworkBridge::Init(App& app) {
  ROSCRAFT_ASSERT(&app == &App::Instance(),
                  "NetworkBridge::Init requires App::Instance()!");
  status_.store(BridgeStatus::kInitializing, std::memory_order_release);

  RCLCPP_INFO(rclcpp::get_logger("NetworkBridge"),
              "Initializing network bridge...");

  InitAsio();

  RCLCPP_INFO(
      rclcpp::get_logger("NetworkBridge"),
      "Network bridge initialized successfully\nListening on udp://%s:%u",
      config_.host.CStr(), config_.port);

  stop_all_stats_producer_ =
      app.IncomingQueue().MakeProducerToken<TopicStatsStopAllCmd>();
  stop_all_echo_producer_ =
      app.IncomingQueue().MakeProducerToken<TopicUnsubscribeCmd>();
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

  status_.store(BridgeStatus::kReady, std::memory_order_release);

  io_thread_ = std::jthread([this] { io_ctx_.run(); });
}

void NetworkBridge::Destroy(App& /*app*/) {
  status_.store(BridgeStatus::kShuttingDown, std::memory_order_release);

  RCLCPP_INFO(rclcpp::get_logger("NetworkBridge"),
              "Destroying network bridge...");

  DestroyAsio();

  {
    std::scoped_lock lock(clients_mutex_);
    clients_.clear();
    clients_last_seen_.clear();
  }

  status_.store(BridgeStatus::kUninitialized, std::memory_order_release);
}

void NetworkBridge::Tick(App& /*app*/) {
  if (Status() != BridgeStatus::kReady) [[unlikely]] {
    return;
  }

  PruneInactiveClients(std::chrono::steady_clock::now());

  DrainAndSendAll();
}

void NetworkBridge::DestroyAsio() {
  if (work_guard_.has_value()) {
    work_guard_->reset();
    work_guard_.reset();
  }
  io_ctx_.stop();

  if (io_thread_.joinable()) {
    io_thread_.join();
  }

  std::error_code ec;
  socket_.close(ec);
}

auto NetworkBridge::ReceiveLoop() -> asio::awaitable<void> {
  ROSCRAFT_ASSERT(Status() == BridgeStatus::kReady,
                  "NetworkBridge is not initialized!");

  auto& app = App::Instance();
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

    const auto now = std::chrono::steady_clock::now();

    // Manage client connections with minimal locking
    if (config_.allow_multiple_connections) {
      // Multi-client mode: update timestamp atomically under shared lock
      std::shared_lock lock(clients_mutex_);
      if (const auto it = clients_last_seen_.find(sender);
          it != clients_last_seen_.end()) {
        it->second.store(now.time_since_epoch().count(),
                         std::memory_order_relaxed);
      } else {
        lock.unlock();
        AddClient(sender, now);
      }

    } else {
      // Single-client mode: only accept from the first connected client
      std::shared_lock lock(clients_mutex_);
      if (clients_.empty()) {
        // Need to register first client
        lock.unlock();
        AddClient(sender, now);
      } else {
        // Check if sender is the registered client
        const auto& registered = *clients_.begin();
        if (sender == registered) {
          if (const auto it = clients_last_seen_.find(sender);
              it != clients_last_seen_.end()) {
            it->second.store(now.time_since_epoch().count(),
                             std::memory_order_relaxed);
          }
        } else {
          // Ignore packets from unknown clients in single-client mode
          RCLCPP_DEBUG(
              rclcpp::get_logger("NetworkBridge"),
              "Ignoring packet from non-active client %s; active client is %s",
              EndpointToString(sender).c_str(),
              EndpointToString(registered).c_str());
          continue;
        }
      }
    }

    HandleDatagram({recv_buf_.data(), n}, sender, app.PendingFrameAllocator());
  }
}

void NetworkBridge::DrainAndSendAll() {
  ROSCRAFT_ASSERT(Status() == BridgeStatus::kReady,
                  "NetworkBridge is not initialized!");

  auto& app = App::Instance();

  // Thread-local FlatBufferBuilder avoids per-message heap allocation.
  thread_local flatbuffers::FlatBufferBuilder fbb(4096);

  // Build a local snapshot of clients under the read lock, then release it
  // before doing any I/O. Using a local (not thread_local) set so that each
  // call sees exactly the clients registered at this moment, with no
  // cross-call bleed-over (which thread_local would cause on Reload()).
  const auto clients_snapshot = BuildClientSnapshot();

  if (clients_snapshot.empty()) {
    return;
  }

  UdpTransport transport(socket_, clients_snapshot);
  UdpPacketSink sink(transport);
  app.HandlerRegistry().DrainAndFlushAll<DrainAndFlushHandlerTypes>(
      app.OutgoingQueue(), sink, fbb);
}

auto NetworkBridge::BuildClientSnapshot() const
    -> std::vector<asio::ip::udp::endpoint> {
  std::shared_lock lock(clients_mutex_);
  return {clients_.begin(), clients_.end()};
}

void NetworkBridge::HandleDatagram(
    std::span<const uint8_t> data, const asio::ip::udp::endpoint& from,
    std::pmr::memory_resource& pending_allocator) {
  ROSCRAFT_ASSERT(Status() == BridgeStatus::kReady,
                  "NetworkBridge is not initialized!");

  flatbuffers::Verifier verifier(data.data(), data.size());
  if (!fbs::VerifyBridgePacketBuffer(verifier)) [[unlikely]] {
    RCLCPP_WARN(rclcpp::get_logger("NetworkBridge"),
                "Dropping malformed datagram (%zu B) from %s!", data.size(),
                from.address().to_string().c_str());
    return;
  }

  auto& app = App::Instance();

  // Dispatch to appropriate handler using arena for transient allocations
  DispatchReceive(app.HandlerRegistry(), app.IncomingQueue(),
                  *fbs::GetBridgePacket(data.data()), pending_allocator);
}

void NetworkBridge::AddClient(const asio::ip::udp::endpoint& client,
                              std::chrono::steady_clock::time_point now) {
  std::scoped_lock lock(clients_mutex_);

  const bool inserted = clients_.insert(client).second;
  clients_last_seen_[client].store(now.time_since_epoch().count(),
                                   std::memory_order_relaxed);

  if (inserted) {
    RCLCPP_INFO(rclcpp::get_logger("NetworkBridge"),
                "Client connected: %s (active clients: %zu)",
                EndpointToString(client).c_str(), clients_.size());
  }
}

void NetworkBridge::RemoveClient(const asio::ip::udp::endpoint& client) {
  std::scoped_lock lock(clients_mutex_);

  const size_t erased = clients_.erase(client);
  clients_last_seen_.erase(client);

  if (erased > 0) {
    RCLCPP_INFO(rclcpp::get_logger("NetworkBridge"),
                "Client disconnected: %s (active clients: %zu)",
                EndpointToString(client).c_str(), clients_.size());
  }
}

void NetworkBridge::PruneInactiveClients(
    std::chrono::steady_clock::time_point now) {
  std::vector<asio::ip::udp::endpoint> stale_clients;

  {
    std::scoped_lock lock(clients_mutex_);
    for (auto it = clients_last_seen_.begin();
         it != clients_last_seen_.end();) {
      const auto last_seen = std::chrono::steady_clock::time_point{
          std::chrono::steady_clock::duration{
              it->second.load(std::memory_order_relaxed)}};
      if (now - last_seen > kClientInactivityTimeout) {
        stale_clients.push_back(it->first);
        clients_.erase(it->first);
        it = clients_last_seen_.erase(it);
      } else {
        ++it;
      }
    }
  }

  for (const auto& client : stale_clients) {
    RCLCPP_INFO(
        rclcpp::get_logger("NetworkBridge"),
        "Client timed out after %llds of inactivity: %s (active clients: %zu)",
        static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(
                                   kClientInactivityTimeout)
                                   .count()),
        EndpointToString(client).c_str(), ClientCount());
  }

  if (!stale_clients.empty() && ClientCount() == 0) {
    auto& app = App::Instance();
    app.IncomingQueue().Enqueue(*stop_all_stats_producer_,
                                TopicStatsStopAllCmd{});
    TopicUnsubscribeCmd unsubscribe;
    app.IncomingQueue().Enqueue(*stop_all_echo_producer_,
                                std::move(unsubscribe));
    RCLCPP_INFO(rclcpp::get_logger("NetworkBridge"),
                "All clients disconnected - stopping all stats and echo "
                "sessions");
  }
}

}  // namespace roscraft::bridge::network
