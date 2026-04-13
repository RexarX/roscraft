#include <doctest/doctest.h>

#include <roscraft/bridge/network/transport.hpp>

#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/udp.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

using namespace roscraft::bridge::network;

namespace {

class DummyEndpoint {
public:
  void Send(std::span<const uint8_t> data) {
    bytes_.assign(data.begin(), data.end());
  }

  [[nodiscard]] auto Bytes() const -> const std::vector<uint8_t>& {
    return bytes_;
  }

private:
  std::vector<uint8_t> bytes_;
};

static_assert(SendEndpoint<DummyEndpoint>);
static_assert(!SendEndpoint<int>);

}  // namespace

TEST_SUITE("bridge::network::SendEndpoint") {
  TEST_CASE("bridge::network::SendEndpoint concept") {
    CHECK(SendEndpoint<DummyEndpoint>);
    CHECK_FALSE(SendEndpoint<int>);
  }
}

TEST_SUITE("bridge::network::UdpTransport") {
  TEST_CASE("bridge::network::UdpTransport::ctor and Send") {
    SUBCASE("Send with no clients is a no-op") {
      asio::io_context io_ctx;
      asio::ip::udp::socket sender(io_ctx);
      sender.open(asio::ip::udp::v4());

      const std::array<asio::ip::udp::endpoint, 0> clients{};
      UdpTransport transport(sender, clients);

      const std::array<uint8_t, 3> payload{1U, 2U, 3U};
      transport.Send(payload);

      CHECK(sender.is_open());
    }

    SUBCASE("Send forwards payload to a single client") {
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

      const std::array<uint8_t, 5> payload{10U, 20U, 30U, 40U, 50U};
      transport.Send(payload);

      std::array<uint8_t, 32> recv_buffer{};
      asio::ip::udp::endpoint from;
      std::error_code ec;
      const size_t n =
          receiver.receive_from(asio::buffer(recv_buffer), from, 0, ec);

      CHECK_FALSE(ec);
      CHECK_EQ(n, payload.size());
      CHECK(std::ranges::equal(std::span(recv_buffer).first(n), payload));
    }

    SUBCASE("Send forwards payload to all snapshot clients") {
      asio::io_context io_ctx;

      asio::ip::udp::socket sender(io_ctx);
      sender.open(asio::ip::udp::v4());

      asio::ip::udp::socket receiver_a(io_ctx);
      receiver_a.open(asio::ip::udp::v4());
      receiver_a.bind(
          asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));

      asio::ip::udp::socket receiver_b(io_ctx);
      receiver_b.open(asio::ip::udp::v4());
      receiver_b.bind(
          asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));

      const std::array<asio::ip::udp::endpoint, 2> clients{
          receiver_a.local_endpoint(), receiver_b.local_endpoint()};
      UdpTransport transport(sender, clients);

      const std::array<uint8_t, 4> payload{7U, 8U, 9U, 10U};
      transport.Send(payload);

      std::array<uint8_t, 32> recv_a{};
      std::array<uint8_t, 32> recv_b{};
      asio::ip::udp::endpoint from_a;
      asio::ip::udp::endpoint from_b;
      std::error_code ec_a;
      std::error_code ec_b;

      const size_t n_a =
          receiver_a.receive_from(asio::buffer(recv_a), from_a, 0, ec_a);
      const size_t n_b =
          receiver_b.receive_from(asio::buffer(recv_b), from_b, 0, ec_b);

      CHECK_FALSE(ec_a);
      CHECK_FALSE(ec_b);
      CHECK_EQ(n_a, payload.size());
      CHECK_EQ(n_b, payload.size());
      CHECK(std::ranges::equal(std::span(recv_a).first(n_a), payload));
      CHECK(std::ranges::equal(std::span(recv_b).first(n_b), payload));
    }
  }
}
