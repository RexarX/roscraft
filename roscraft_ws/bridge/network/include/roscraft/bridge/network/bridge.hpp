#pragma once

#include <roscraft/bridge/bridge.hpp>
#include <roscraft/bridge/network/command/handler_registry.hpp>
#include <roscraft/bridge/network/config.hpp>
#include <roscraft/bridge/network/transport.hpp>

#include <asio/awaitable.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory_resource>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace roscraft::bridge::network {

/// @brief Network bridge implementation using UDP sockets.
/// @details Manages bidirectional communication between Minecraft mod and ROS2.
///
/// Architecture:
/// - Uses DoubleFrameAllocator for zero-allocation packet handling
/// - Frame 0 (current): actively being processed (receive/send)
/// - Frame 1 (pending): reserved for next cycle
/// - After each send cycle, frames are swapped via Advance()
///
/// Thread Safety:
/// - ReceiveLoop runs on ASIO io_context thread
/// - clients_ protected by shared_mutex for read/write separation
/// - bridge nodes run on ROS spin thread
class NetworkBridge final : public Bridge {
public:
  /// @brief Construct a new NetworkBridge.
  /// @param config Network configuration (host, port, connection mode)
  explicit NetworkBridge(BridgeConfig config = BridgeConfig{})
      : config_(std::move(config)), io_ctx_(1), socket_(io_ctx_) {}
  ~NetworkBridge() override;

  /// @brief Initialize the bridge.
  /// @param app Application instance for queue access
  void Init(App& app) override;

  /// @brief Destroy the bridge and release resources.
  /// @param app Application instance
  void Destroy(App& app) override;

  /// @brief Reload the bridge (destroy + init).
  /// @param app Application instance
  void Reload(App& app) override;

  /// @brief Runs one bridge tick.
  /// @param app Application instance
  void Tick(App& app) override;

  /// @brief Get number of registered clients.
  /// @return Client count
  [[nodiscard]] size_t ClientCount() const;

  /// @brief Get current bridge status.
  /// @return Current status
  [[nodiscard]] BridgeStatus Status() const noexcept override {
    return status_.load(std::memory_order_relaxed);
  }

  /// @brief Get current configuration.
  /// @return Const reference to configuration
  [[nodiscard]] const BridgeConfig& Config() const noexcept { return config_; }

private:
  /// @brief Register all packet handlers with the registry.
  void InitCommandHandlerRegistry();

  /// @brief Initialize ASIO networking.
  void InitAsio();

  /// @brief Destroy ASIO networking.
  void DestroyAsio();

  /// @brief Main receive coroutine - handles incoming UDP datagrams.
  [[nodiscard]] auto ReceiveLoop() -> asio::awaitable<void>;

  /// @brief Drain outgoing queue and send to all registered clients.
  void DrainAndSendAll();

  /// @brief Creates a stable snapshot of current clients.
  /// @return Vector of current clients
  [[nodiscard]] auto BuildClientSnapshot() const
      -> std::vector<asio::ip::udp::endpoint>;

  /// @brief Process received datagram.
  /// @param data Raw packet data
  /// @param from Sender endpoint
  /// @param pending_allocator Pending frame allocator
  void HandleDatagram(std::span<const uint8_t> data,
                      const asio::ip::udp::endpoint& from,
                      std::pmr::memory_resource& pending_allocator);

  /// @brief Add client endpoint to registered clients.
  /// @param client Client endpoint to add
  void AddClient(const asio::ip::udp::endpoint& client,
                 std::chrono::steady_clock::time_point now);

  /// @brief Update last-seen timestamp for a client.
  /// @param client Client endpoint
  /// @param now Current steady clock time
  void MarkClientSeen(const asio::ip::udp::endpoint& client,
                      std::chrono::steady_clock::time_point now);

  /// @brief Remove client endpoint from registered clients.
  /// @param client Client endpoint to remove
  void RemoveClient(const asio::ip::udp::endpoint& client);

  /// @brief Remove inactive clients based on timeout.
  /// @param now Current steady clock time
  void PruneInactiveClients(std::chrono::steady_clock::time_point now);

  // ---- Configuration --------------------------------------------------------

  BridgeConfig config_;

  // ---- Lifecycle state ------------------------------------------------------

  std::atomic<BridgeStatus> status_{BridgeStatus::kUninitialized};

  // ---- Handler registry -----------------------------------------------------

  CommandHandlerRegistry registry_;

  // ---- ASIO networking ------------------------------------------------------

  asio::io_context io_ctx_;
  asio::ip::udp::socket socket_;

  using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;
  std::optional<WorkGuard> work_guard_;

  // ---- Client management (thread-safe) -------------------------------------

  mutable std::shared_mutex clients_mutex_;
  std::unordered_set<asio::ip::udp::endpoint> clients_;
  std::unordered_map<asio::ip::udp::endpoint,
                     std::chrono::steady_clock::time_point>
      clients_last_seen_;

  static constexpr auto kClientInactivityTimeout = std::chrono::seconds(30);

  // ---- Receive-side resources ----------------------------------------------

  /// Maximum UDP datagram size (excluding IP/UDP headers).
  static constexpr size_t kMaxDatagramSize = 65507;

  /// Receive buffer reused for each datagram.
  std::array<uint8_t, kMaxDatagramSize> recv_buf_{};
};

inline NetworkBridge::~NetworkBridge() {
  if (Status() == BridgeStatus::kUninitialized) {
    return;
  }

  DestroyAsio();
}

inline void NetworkBridge::Reload(App& app) {
  Destroy(app);
  Init(app);
}

inline size_t NetworkBridge::ClientCount() const {
  std::shared_lock lock(clients_mutex_);
  return clients_.size();
}

}  // namespace roscraft::bridge::network
