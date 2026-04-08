#pragma once

#include <asio/ip/udp.hpp>

#include <rclcpp/logging.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <span>

namespace roscraft::bridge::network {

/// @brief Concept for transport endpoints used by command handlers.
/// @tparam T Transport endpoint type
template <typename T>
concept SendEndpoint = requires(T& endpoint, std::span<const uint8_t> data) {
  { endpoint.Send(data) } -> std::same_as<void>;
};

/// @brief UDP transport wrapper used by network command handlers.
/// @details Sends serialized packets to all currently registered clients.
class UdpTransport {
public:
  /// @brief Constructs a UDP transport endpoint.
  /// @param socket UDP socket used for sends
  /// @param clients Snapshot of current clients
  UdpTransport(asio::ip::udp::socket& socket,
               std::span<const asio::ip::udp::endpoint> clients)
      : socket_(socket), clients_(clients) {}

  /// @brief Sends payload to all snapshot clients.
  /// @param data Serialized payload bytes
  void Send(std::span<const uint8_t> data);

private:
  std::reference_wrapper<asio::ip::udp::socket> socket_;
  std::span<const asio::ip::udp::endpoint> clients_;
};

inline void UdpTransport::Send(std::span<const uint8_t> data) {
  auto& socket = socket_.get();
  const auto payload = asio::buffer(data.data(), data.size());
  for (const auto& client : clients_) {
    std::error_code ec;
    socket.send_to(payload, client, 0, ec);
    if (ec) {
      RCLCPP_WARN(rclcpp::get_logger("NetworkBridge"), "Send error to %s: %s!",
                  client.address().to_string().c_str(), ec.message().c_str());
    }
  }
}

}  // namespace roscraft::bridge::network
