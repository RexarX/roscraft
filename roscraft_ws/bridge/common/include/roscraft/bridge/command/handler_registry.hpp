#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/handler.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/container/multi_type_map.hpp>
#include <roscraft/container/typed_buffer.hpp>
#include <roscraft/generated/bridge_packets.hpp>
#include <roscraft/utils/type_info.hpp>

#include <flatbuffers/flatbuffers.h>

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory_resource>
#include <tuple>
#include <type_traits>
#include <utility>

namespace roscraft::bridge {

/// @brief Type-keyed registry of command handlers.
/// @details Stores heterogeneous handler instances by type and provides
/// dispatch methods for both the receive path (incoming packets) and the drain
/// path (outgoing commands -> serialized packets via any `PacketSink`).
///
/// The registry is non-copyable but movable.  Handlers are stored in-place
/// via `container::MultiTypeMap<TypedBuffer<>>`.
class CommandHandlerRegistry final {
public:
  CommandHandlerRegistry() = default;
  CommandHandlerRegistry(const CommandHandlerRegistry&) = delete;
  CommandHandlerRegistry(CommandHandlerRegistry&&) = default;
  ~CommandHandlerRegistry() = default;

  CommandHandlerRegistry& operator=(const CommandHandlerRegistry&) = delete;
  CommandHandlerRegistry& operator=(CommandHandlerRegistry&&) = default;

  /// @brief Removes all registered handlers.
  void Clear() noexcept { map_.ResetAll(); }

  /// @brief Adds or replaces a handler.
  /// @tparam T Handler type satisfying `CommandHandler`
  /// @param handler Handler instance to store
  template <CommandHandler T>
  void AddHandler(T&& handler);

  /// @brief Adds a handler only if no handler of type `T` is already present.
  /// @tparam T Handler type satisfying `CommandHandler`
  /// @param handler Handler instance to store
  /// @return `true` if inserted, `false` if a handler already existed
  template <CommandHandler T>
  bool TryAddHandler(T&& handler);

  /// @brief Constructs a handler in-place, replacing any existing one.
  /// @tparam T Handler type satisfying `CommandHandler`
  /// @tparam Args Constructor argument types
  /// @param args Arguments forwarded to `T`'s constructor
  /// @return Reference to the newly constructed handler
  template <CommandHandler T, typename... Args>
    requires std::constructible_from<T, Args...>
  T& EmplaceHandler(Args&&... args);

  /// @brief Constructs a handler in-place only if absent.
  /// @tparam T Handler type satisfying `CommandHandler`
  /// @tparam Args Constructor argument types
  /// @param args Arguments forwarded to `T`'s constructor
  /// @return Pair of (reference to handler, inserted flag)
  template <CommandHandler T, typename... Args>
    requires std::constructible_from<T, Args...>
  auto TryEmplaceHandler(Args&&... args)
      -> std::pair<std::reference_wrapper<T>, bool>;

  /// @brief Removes the handler for type `T`.
  /// @warning Triggers assertion if no handler of type `T` is registered.
  /// @tparam T Handler type satisfying `CommandHandler`
  template <CommandHandler T>
  void RemoveHandler() noexcept;

  /// @brief Removes the handler for type `T` if present.
  /// @tparam T Handler type satisfying `CommandHandler`
  /// @return `true` if removed, `false` if no handler was registered
  template <CommandHandler T>
  bool TryRemoveHandler() noexcept {
    return map_.Remove<T>();
  }

  /// @brief Drains and flushes every handler whose type appears in a
  /// `std::tuple` type-list.
  /// @tparam TupleT `std::tuple<H1, H2, …>` where each `Hi` satisfies
  /// `CommandHandlerWithDrain`
  /// @tparam Sink Packet sink type satisfying `PacketSink`
  /// @param out Outgoing command queue
  /// @param sink Destination for serialized packets
  /// @param fbb  Reusable FlatBufferBuilder
  template <typename TupleT, PacketSink Sink>
  void DrainAndFlushAll(CommandQueue& out, Sink& sink,
                        flatbuffers::FlatBufferBuilder& fbb) {
    DrainAndFlushAllImpl(out, sink, fbb, std::type_identity<TupleT>{});
  }

  /// @brief Drains and flushes an explicit parameter pack of handler types.
  /// @tparam Ts Handler types (each satisfies `CommandHandlerWithDrain`)
  /// @tparam Sink Packet sink type satisfying `PacketSink`
  /// @param out Outgoing command queue
  /// @param sink Destination for serialized packets
  /// @param fbb Reusable FlatBufferBuilder
  template <CommandHandlerWithDrain... Ts, PacketSink Sink>
  void DrainAndFlushAll(CommandQueue& out, Sink& sink,
                        flatbuffers::FlatBufferBuilder& fbb) {
    (DrainAndFlushIfExists<Ts>(out, sink, fbb), ...);
  }

  /// @brief Drains and flushes the handler for type `T`.
  /// @warning Triggers assertion if no handler of type `T` is registered.
  /// @tparam T Handler type satisfying `CommandHandlerWithDrain`
  /// @tparam Sink Packet sink type satisfying `PacketSink`
  /// @param out Outgoing command queue
  /// @param sink Destination for serialized packets
  /// @param fbb Reusable FlatBufferBuilder
  template <CommandHandlerWithDrain T, PacketSink Sink>
  void DrainAndFlush(CommandQueue& out, Sink& sink,
                     flatbuffers::FlatBufferBuilder& fbb);

  /// @brief Drains and flushes the handler for type `T` if registered.
  /// @tparam T Handler type satisfying `CommandHandlerWithDrain`
  /// @tparam Sink Packet sink type satisfying `PacketSink`
  /// @param out Outgoing command queue
  /// @param sink Destination for serialized packets
  /// @param fbb Reusable FlatBufferBuilder
  template <CommandHandlerWithDrain T, PacketSink Sink>
  void DrainAndFlushIfExists(CommandQueue& out, Sink& sink,
                             flatbuffers::FlatBufferBuilder& fbb);

  /// @brief Dispatches an incoming packet to the handler for type `T`.
  /// @warning Triggers assertion if no handler of type `T` is registered.
  /// @tparam T Handler type satisfying `CommandHandlerWithReceive`
  /// @param in Incoming command queue
  /// @param pkt Decoded bridge packet
  /// @param arena Frame-scoped memory resource
  template <CommandHandlerWithReceive T>
  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  /// @brief Checks if a handler for type `T` is registered.
  /// @tparam T Handler type satisfying `CommandHandler`
  /// @return `true` if a handler is registered, `false` otherwise
  template <CommandHandler T>
  [[nodiscard]] bool Contains() const noexcept {
    return map_.Contains<T>();
  }

  /// @brief Checks if no handlers are registered.
  /// @return `true` if no handlers are registered, `false` otherwise
  [[nodiscard]] bool Empty() const noexcept { return map_.TypeCount() == 0; }

  /// @brief Checks the number of registered handlers.
  /// @return The number of registered handlers
  [[nodiscard]] size_t Size() const noexcept { return map_.TypeCount(); }

private:
  template <typename... Ts, PacketSink Sink>
  void DrainAndFlushAllImpl(CommandQueue& out, Sink& sink,
                            flatbuffers::FlatBufferBuilder& fbb,
                            std::type_identity<std::tuple<Ts...>>) {
    (DrainAndFlushIfExists<Ts>(out, sink, fbb), ...);
  }

  using Storage = container::TypedBuffer<>;
  container::MultiTypeMap<Storage> map_;
};

template <CommandHandler T>
inline void CommandHandlerRegistry::AddHandler(T&& handler) {
  using H = std::remove_cvref_t<T>;
  auto& storage = map_.Ensure<H>();
  storage.template Set<H>(std::forward<T>(handler));
}

template <CommandHandler T>
inline bool CommandHandlerRegistry::TryAddHandler(T&& handler) {
  using H = std::remove_cvref_t<T>;
  if (map_.Contains<H>()) {
    return false;
  }
  auto& storage = map_.Ensure<H>();
  storage.template Set<H>(std::forward<T>(handler));
  return true;
}

template <CommandHandler T, typename... Args>
  requires std::constructible_from<T, Args...>
inline T& CommandHandlerRegistry::EmplaceHandler(Args&&... args) {
  auto& storage = map_.Ensure<T>();
  return storage.template Set<T>(std::forward<Args>(args)...);
}

template <CommandHandler T, typename... Args>
  requires std::constructible_from<T, Args...>
inline auto CommandHandlerRegistry::TryEmplaceHandler(Args&&... args)
    -> std::pair<std::reference_wrapper<T>, bool> {
  if (map_.Contains<T>()) {
    return {map_.Get<T>().template Value<T>(), false};
  }
  auto& inserted =
      map_.Ensure<T>().template Set<T>(std::forward<Args>(args)...);
  return {inserted, true};
}

template <CommandHandler T>
inline void CommandHandlerRegistry::RemoveHandler() noexcept {
  ROSCRAFT_ASSERT(map_.Contains<T>(), "Handler '{}' does not exist!",
                  utils::TypeNameOf<T>());
  map_.Remove<T>();
}

template <CommandHandlerWithDrain T, PacketSink Sink>
inline void CommandHandlerRegistry::DrainAndFlush(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  ROSCRAFT_ASSERT(map_.Contains<T>(), "Handler '{}' does not exist!",
                  utils::TypeNameOf<T>());
  auto& handler = map_.Get<T>().template Value<T>();
  handler.DrainAndFlush(out, sink, fbb);
}

template <CommandHandlerWithDrain T, PacketSink Sink>
inline void CommandHandlerRegistry::DrainAndFlushIfExists(
    CommandQueue& out, Sink& sink, flatbuffers::FlatBufferBuilder& fbb) {
  auto* storage = map_.TryGet<T>();
  if (storage == nullptr) {
    return;
  }
  auto& handler = storage->template Value<T>();
  handler.DrainAndFlush(out, sink, fbb);
}

template <CommandHandlerWithReceive T>
inline void CommandHandlerRegistry::Receive(CommandQueue& in,
                                            const fbs::BridgePacket& pkt,
                                            std::pmr::memory_resource& arena) {
  ROSCRAFT_ASSERT(map_.Contains<T>(), "Handler '{}' does not exist!",
                  utils::TypeNameOf<T>());
  auto& handler = map_.Get<T>().template Value<T>();
  if constexpr (T::kReceiveType != fbs::PacketPayload::NONE) {
    if (pkt.payload_type() == T::kReceiveType) [[likely]] {
      handler.Receive(in, pkt, arena);
    }
  }
}

}  // namespace roscraft::bridge
