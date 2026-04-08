#pragma once

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/network/transport.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <type_traits>

namespace roscraft {

namespace bridge {

namespace network {

/// @brief Concept for command handlers.
/// @details Requires the type to be destructible, not polymorphic,
/// and have alignment less than or equal to `std::max_align_t`.
template <typename T>
concept CommandHandler =
    std::destructible<T> && !std::is_polymorphic_v<std::remove_cvref_t<T>> &&
    (alignof(std::remove_cvref_t<T>) <= alignof(std::max_align_t));

/// @brief Concept for command handlers with a `Receive` method.
/// @details Requires the type to have a
/// `Receive(roscraft::bridge::CommandQueue&, const fbs::BridgePacket& pkt,
/// std::pmr::memory_resource&) -> void` method and
/// `static constexpr PacketPayloadType kReceiveType` variable.
template <typename T>
concept CommandHandlerWithReceive =
    CommandHandler<T> &&
    requires(T& handler, CommandQueue& in, const fbs::BridgePacket& pkt,
             std::pmr::memory_resource& arena) {
      {
        std::remove_cvref_t<T>::kReceiveType
      } -> std::convertible_to<fbs::PacketPayload>;
      { handler.Receive(in, pkt, arena) } -> std::same_as<void>;
    };

/// @brief Concept for command handlers with a `DrainAndSend` method.
/// @details Requires the type to have a
/// `DrainAndSend(roscraft::bridge::CommandQueue&, SendEndpoint,
/// flatbuffers::FlatBufferBuilder&) -> void` method.
template <typename T>
concept CommandHandlerWithDrainAndSend =
    CommandHandler<T> &&
    requires(T& handler, CommandQueue& out, UdpTransport& transport,
             flatbuffers::FlatBufferBuilder& fbb) {
      { handler.DrainAndSend(out, transport, fbb) } -> std::same_as<void>;
    };

}  // namespace network

}  // namespace bridge

}  // namespace roscraft
