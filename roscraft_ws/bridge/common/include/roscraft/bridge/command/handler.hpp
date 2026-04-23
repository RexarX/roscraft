#pragma once

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <flatbuffers/flatbuffers.h>

#include <concepts>
#include <memory_resource>
#include <type_traits>

namespace roscraft::bridge {

/// @brief Base concept for all command handler types.
/// @details A handler must be destructible, move-constructible,
/// non-polymorphic, and have alignment no greater than `std::max_align_t`.
template <typename T>
concept CommandHandler =
    std::destructible<T> && std::move_constructible<T> &&
    !std::is_polymorphic_v<std::remove_cvref_t<T>> &&
    (alignof(std::remove_cvref_t<T>) <= alignof(std::max_align_t));

/// @brief Concept for handlers that can receive incoming packets (client ->
/// ROS).
/// @details A receive handler must provide:
/// - `static constexpr fbs::PacketPayload kReceiveType`
/// - `void Receive(CommandQueue&, const fbs::BridgePacket&,
///                 std::pmr::memory_resource&)`
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

/// @brief Concept for a packet sink that accepts a completed
/// `FlatBufferBuilder`.
/// @details After a handler finishes building a FlatBuffers packet, it calls
/// `sink.Send(fbb)` to forward the finished bytes. Concrete implementations
/// may send over UDP (`UdpPacketSink`) or deliver via JNI (`JniPacketSink`).
template <typename T>
concept PacketSink = requires(T& sink, flatbuffers::FlatBufferBuilder& fbb) {
  { sink.Send(fbb) } -> std::same_as<void>;
};

/// @brief Concept for handlers that can drain outgoing commands and flush them
/// via any `PacketSink`.
/// @details A drain handler must expose:
/// @code
/// template <PacketSink Sink>
/// void DrainAndFlush(CommandQueue& out, Sink& sink,
///                    flatbuffers::FlatBufferBuilder& fbb);
/// @endcode
/// Because the method is templated on `Sink`, the concept is checked by
/// verifying that the method is callable with a minimal concrete probe type.
struct PacketSinkProbe final {
  void Send(flatbuffers::FlatBufferBuilder&) {}
};

template <typename T>
concept CommandHandlerWithDrain =
    CommandHandler<T> &&
    requires(T& handler, CommandQueue& out, PacketSinkProbe& sink,
             flatbuffers::FlatBufferBuilder& fbb) {
      { handler.DrainAndFlush(out, sink, fbb) } -> std::same_as<void>;
    };

}  // namespace roscraft::bridge
