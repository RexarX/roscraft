#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/network/command/handler.hpp>
#include <roscraft/bridge/network/transport.hpp>
#include <roscraft/container/multi_type_map.hpp>
#include <roscraft/container/typed_buffer.hpp>
#include <roscraft/generated/bridge_packets_generated.hpp>

#include <flatbuffers/flatbuffers.h>

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory_resource>
#include <tuple>
#include <type_traits>
#include <utility>

namespace roscraft {

namespace bridge {

namespace network {

class CommandHandlerRegistry {
public:
  CommandHandlerRegistry() = default;
  ~CommandHandlerRegistry() = default;

  /// @brief Adds a handler for a command type.
  /// @details If a handler for the given type already exists, it will be
  /// replaced.
  /// @tparam T Command handler type
  /// @param handler Handler to add
  template <CommandHandler T>
  void AddHandler(T&& handler);

  /// @brief Tries to add a handler for a command type.
  /// @details If a handler for the given type already exists, it will not be
  /// replaced.
  /// @tparam T Command handler type
  /// @param handler Handler to add
  /// @return `true` if the handler was added, `false` if it already existed
  template <CommandHandler T>
  bool TryAddHandler(T&& handler);

  /// @brief Emplaces a handler for a command type.
  /// @details If a handler for the given type already exists, it will be
  /// replaced.
  /// @tparam T Command handler type
  /// @param args Arguments to construct the handler
  template <CommandHandler T, typename... Args>
    requires std::constructible_from<T, Args...>
  T& EmplaceHandler(Args&&... args);

  /// @brief Tries to emplace a handler for a command type.
  /// @details If a handler for the given type already exists, it will not be
  /// replaced.
  /// @tparam T Command handler type
  /// @param args Arguments to construct the handler
  /// @return A pair containing a reference to the handler and a boolean
  /// indicating whether the handler was added (true if it was added, false if
  /// it already existed)
  template <CommandHandler T, typename... Args>
    requires std::constructible_from<T, Args...>
  auto TryEmplaceHandler(Args&&... args)
      -> std::pair<std::reference_wrapper<T>, bool>;

  /// @brief Removes a handler for a command type.
  /// @warning Triggers assertion if the handler does not exist
  /// @tparam T Command handler type
  template <CommandHandler T>
  void RemoveHandler() noexcept;

  /// @brief Tries to remove a handler for a command type.
  /// @tparam T Command handler type
  /// @return `true` if the handler was removed, `false` if it did not exist
  template <CommandHandler T>
  bool TryRemoveHandler() noexcept {
    return map_.Remove<T>() > 0;
  }

  /// @brief Drains and sends all command packets using a tuple type list.
  /// @details Call as `DrainAndSendAll<DrainAndSendHandlerTypes>(...)`.
  /// @tparam TupleT `std::tuple<Handler1, Handler2, ...>` of handler types
  /// @param out Command queue to drain from
  /// @param transport Transport endpoint
  /// @param fbb FlatBufferBuilder to reuse
  template <typename TupleT>
  void DrainAndSendAll(CommandQueue& out, UdpTransport& transport,
                       flatbuffers::FlatBufferBuilder& fbb) {
    DrainAndSendAllImpl(out, transport, fbb, std::type_identity<TupleT>{});
  }

  /// @brief Drains and sends all command packets.
  /// @details Expand a parameter pack of handler types directly.
  /// @tparam Ts Command handler types
  /// @param out Command queue to drain from
  /// @param transport Transport endpoint
  /// @param fbb FlatBufferBuilder to reuse (thread-local)
  template <CommandHandlerWithDrainAndSend... Ts>
  void DrainAndSendAll(CommandQueue& out, UdpTransport& transport,
                       flatbuffers::FlatBufferBuilder& fbb) {
    (DrainAndSendIfExists<Ts>(out, transport, fbb), ...);
  }

  /// @brief Drains and sends a command packet.
  /// @warning Triggers assertion if the handler does not exist.
  /// @tparam T Command handler type
  /// @param out Command queue to drain from
  /// @param transport Transport endpoint
  /// @param fbb FlatBufferBuilder to reuse
  template <CommandHandlerWithDrainAndSend T>
  void DrainAndSend(CommandQueue& out, UdpTransport& transport,
                    flatbuffers::FlatBufferBuilder& fbb);

  /// @brief Drains and sends a command packet if a handler exists.
  /// @tparam T Command handler type
  /// @param out Command queue to drain from
  /// @param transport Transport endpoint
  /// @param fbb FlatBufferBuilder to reuse
  template <CommandHandlerWithDrainAndSend T>
  void DrainAndSendIfExists(CommandQueue& out, UdpTransport& transport,
                            flatbuffers::FlatBufferBuilder& fbb);

  /// @brief Receives a command packet.
  /// @warning Triggers assertion if the handler does not exist.
  /// @tparam T Command handler type
  /// @param in Command queue to receive into
  /// @param pkt Packet to receive
  /// @param arena Frame allocator
  template <CommandHandlerWithReceive T>
  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  /// @brief Checks if a handler for a command type exists.
  /// @tparam T Command handler type
  /// @return `true` if the handler exists, `false` otherwise
  template <CommandHandler T>
  [[nodiscard]] constexpr bool Contains() const noexcept {
    return map_.Contains<T>();
  }

  /// @brief Checks if the registry is empty.
  /// @return `true` if the registry is empty, `false` otherwise
  [[nodiscard]] constexpr bool Empty() const noexcept {
    return map_.TypeCount() == 0;
  }

  /// @brief Returns the number of handlers in the registry.
  /// @return The number of handlers in the registry
  [[nodiscard]] constexpr size_t Size() const noexcept { return map_.Size(); }

private:
  /// @brief Implementation detail — expands a `std::tuple` type list into a
  /// parameter pack and calls `DrainAndSendIfExists` for each handler.
  template <typename... Ts>
  void DrainAndSendAllImpl(CommandQueue& out, UdpTransport& transport,
                           flatbuffers::FlatBufferBuilder& fbb,
                           std::type_identity<std::tuple<Ts...>>) {
    (DrainAndSendIfExists<Ts>(out, transport, fbb), ...);
  }

  using Storage = container::TypedBuffer<>;
  container::MultiTypeMap<Storage> map_;
};

template <CommandHandler T>
inline void CommandHandlerRegistry::AddHandler(T&& handler) {
  using HandlerType = std::remove_cvref_t<T>;
  auto& storage = map_.Ensure<HandlerType>();
  storage.template Set<HandlerType>(std::forward<T>(handler));
}

/// @brief Tries to add a handler for a command type.
/// @details If a handler for the given type already exists, it will not be
/// replaced.
/// @tparam T Command handler type
/// @param handler Handler to add
/// @return `true` if the handler was added, `false` if it already existed
template <CommandHandler T>
inline bool CommandHandlerRegistry::TryAddHandler(T&& handler) {
  using HandlerType = std::remove_cvref_t<T>;
  if (map_.Contains<HandlerType>()) {
    return false;
  }
  auto& storage = map_.Ensure<HandlerType>();
  storage.template Set<HandlerType>(std::forward<T>(handler));
  return true;
}

/// @brief Emplaces a handler for a command type.
/// @details If a handler for the given type already exists, it will be
/// replaced.
/// @tparam T Command handler type
/// @param args Arguments to construct the handler
template <CommandHandler T, typename... Args>
  requires std::constructible_from<T, Args...>
inline T& CommandHandlerRegistry::EmplaceHandler(Args&&... args) {
  auto& storage = map_.Ensure<T>();
  return storage.template Set<T>(std::forward<Args>(args)...);
}

/// @brief Tries to emplace a handler for a command type.
/// @details If a handler for the given type already exists, it will not be
/// replaced.
/// @tparam T Command handler type
/// @param args Arguments to construct the handler
/// @return A pair containing a reference to the handler and a boolean
/// indicating whether the handler was added (true if it was added, false if
/// it already existed)
template <CommandHandler T, typename... Args>
  requires std::constructible_from<T, Args...>
inline auto CommandHandlerRegistry::TryEmplaceHandler(Args&&... args)
    -> std::pair<std::reference_wrapper<T>, bool> {
  if (map_.Contains<T>()) {
    auto& existing = map_.Get<T>().template Value<T>();
    return {existing, false};
  }
  auto& inserted =
      map_.Ensure<T>().template Set<T>(std::forward<Args>(args)...);
  return {inserted, true};
}

template <CommandHandler T>
inline void CommandHandlerRegistry::RemoveHandler() noexcept {
  ROSCRAFT_ASSERT(map_.Contains<T>(), "Handler does not exist!");
  map_.Remove<T>();
}

template <CommandHandlerWithDrainAndSend T>
inline void CommandHandlerRegistry::DrainAndSendIfExists(
    CommandQueue& out, UdpTransport& transport,
    flatbuffers::FlatBufferBuilder& fbb) {
  auto* storage = map_.TryGet<T>();
  if (storage == nullptr) [[unlikely]] {
    return;
  }
  auto& handler = storage->template Value<T>();
  handler.DrainAndSend(out, transport, fbb);
}

template <CommandHandlerWithDrainAndSend T>
inline void CommandHandlerRegistry::DrainAndSend(
    CommandQueue& out, UdpTransport& transport,
    flatbuffers::FlatBufferBuilder& fbb) {
  ROSCRAFT_ASSERT(map_.Contains<T>(), "Handler does not exist!");
  auto& handler = map_.Get<T>().template Value<T>();
  handler.DrainAndSend(out, transport, fbb);
}

template <CommandHandlerWithReceive T>
inline void CommandHandlerRegistry::Receive(CommandQueue& in,
                                            const fbs::BridgePacket& pkt,
                                            std::pmr::memory_resource& arena) {
  ROSCRAFT_ASSERT(map_.Contains<T>(), "Handler does not exist!");
  auto& handler = map_.Get<T>().template Value<T>();
  if constexpr (handler.kReceiveType != fbs::PacketPayload::NONE) {
    if (pkt.payload_type() == handler.kReceiveType) [[likely]] {
      handler.Receive(in, pkt, arena);
    }
  }
}

}  // namespace network

}  // namespace bridge

}  // namespace roscraft
