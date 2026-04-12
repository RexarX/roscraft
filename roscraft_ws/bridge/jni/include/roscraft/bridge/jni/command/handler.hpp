#pragma once

#include <roscraft/generated/bridge_packets_generated.hpp>

#include <flatbuffers/flatbuffers.h>

#include <jni.h>

#include <concepts>
#include <memory_resource>
#include <span>
#include <type_traits>

namespace roscraft::bridge {

class CommandQueue;

namespace jni {

class BridgeCallback;

/// @brief Concept for JNI command handlers.
/// @details A handler must be destructible, move-constructible, and be an
/// object.
template <typename T>
concept CommandHandler = std::destructible<T> && std::move_constructible<T> &&
                         std::is_object_v<std::remove_cvref_t<T>>;

/// @brief Concept for JNI command handlers that can receive incoming commands
/// (mod -> ROS) from a JNI call.
/// @details A receive handler must provide:
/// - `static constexpr fbs::PacketPayload kReceiveType`.
/// - `void Receive(CommandQueue&, const fbs::BridgePacket&,
/// std::pmr::memory_resource&)`.
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

/// @brief Concept for JNI command handlers that can drain and deliver commands
/// (ROS -> mod) to a JNI call.
/// @details A drain handler must provide:
/// - `void DrainAndDeliver(CommandQueue&, JNIEnv*, const BridgeCallback&,
///   flatbuffers::FlatBufferBuilder&)`.
template <typename T>
concept CommandHandlerWithDrainAndDeliver =
    CommandHandler<T> && requires(T& handler, CommandQueue& out, JNIEnv* env,
                                  const BridgeCallback& callback,
                                  flatbuffers::FlatBufferBuilder& fbb) {
      {
        handler.DrainAndDeliver(out, env, callback, fbb)
      } -> std::same_as<void>;
    };

}  // namespace jni

}  // namespace roscraft::bridge
