#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/jni/command/callback.hpp>
#include <roscraft/bridge/jni/command/handler.hpp>
#include <roscraft/container/multi_type_map.hpp>
#include <roscraft/container/typed_buffer.hpp>
#include <roscraft/utils/type_info.hpp>

#include <flatbuffers/flatbuffers.h>

#include <jni.h>

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory_resource>
#include <tuple>
#include <type_traits>
#include <utility>

namespace roscraft::bridge {

class CommandQueue;

namespace jni {

class CommandHandlerRegistry {
public:
  CommandHandlerRegistry() = default;
  CommandHandlerRegistry(const CommandHandlerRegistry&) = delete;
  CommandHandlerRegistry(CommandHandlerRegistry&&) = default;
  ~CommandHandlerRegistry() = default;

  CommandHandlerRegistry& operator=(const CommandHandlerRegistry&) = delete;
  CommandHandlerRegistry& operator=(CommandHandlerRegistry&&) = default;

  /// @brief Clears all registered handlers.
  void Clear() noexcept { map_.ResetAll(); }

  /// @brief Adds or replaces a handler.
  /// @tparam T Handler type
  /// @param handler Handler instance
  template <CommandHandler T>
  void AddHandler(T&& handler);

  /// @brief Tries to add a handler if absent.
  /// @tparam T Handler type
  /// @param handler Handler instance
  /// @return `true` when inserted, `false` otherwise
  template <CommandHandler T>
  bool TryAddHandler(T&& handler);

  /// @brief Emplaces and replaces a handler.
  /// @tparam T Handler type
  /// @param args Constructor arguments
  /// @return Reference to inserted handler
  template <CommandHandler T, typename... Args>
    requires std::constructible_from<T, Args...>
  T& EmplaceHandler(Args&&... args);

  /// @brief Tries to emplace a handler.
  /// @tparam T Handler type
  /// @param args Constructor arguments
  /// @return Pair of handler ref and inserted flag
  template <CommandHandler T, typename... Args>
    requires std::constructible_from<T, Args...>
  auto TryEmplaceHandler(Args&&... args)
      -> std::pair<std::reference_wrapper<T>, bool>;

  /// @brief Removes handler for type `T`.
  /// @warning Triggers assertion if handler does not exist.
  /// @tparam T Handler type
  template <CommandHandler T>
  void RemoveHandler() noexcept;

  /// @brief Tries to remove handler for type `T`.
  /// @tparam T Handler type
  /// @return `true` when removed, `false` otherwise
  template <CommandHandler T>
  bool TryRemoveHandler() noexcept {
    return map_.Remove<T>();
  }

  /// @brief Drains and delivers all handlers from tuple type-list.
  /// @tparam TupleT `std::tuple<...>` of drain-capable handlers
  /// @param out Outgoing queue
  /// @param env JNI environment
  /// @param callback Java callback dispatcher
  /// @param fbb FlatBufferBuilder reuse buffer
  template <typename TupleT>
  void DrainAndDeliverAll(CommandQueue& out, JNIEnv* env,
                          const BridgeCallback& callback,
                          flatbuffers::FlatBufferBuilder& fbb) {
    DrainAndDeliverAllImpl(out, env, callback, fbb,
                           std::type_identity<TupleT>{});
  }

  /// @brief Drains and delivers explicit handler parameter pack.
  /// @tparam Ts Handler types
  /// @param out Outgoing queue
  /// @param env JNI environment
  /// @param callback Java callback dispatcher
  /// @param fbb FlatBufferBuilder reuse buffer
  template <CommandHandlerWithDrainAndDeliver... Ts>
  void DrainAndDeliverAll(CommandQueue& out, JNIEnv* env,
                          const BridgeCallback& callback,
                          flatbuffers::FlatBufferBuilder& fbb) {
    (DrainAndDeliverIfExists<Ts>(out, env, callback, fbb), ...);
  }

  /// @brief Drains and delivers one handler.
  /// @warning Triggers assertion if handler does not exist.
  /// @tparam T Handler type
  /// @param out Outgoing queue
  /// @param env JNI environment
  /// @param callback Java callback dispatcher
  /// @param fbb FlatBufferBuilder reuse buffer
  template <CommandHandlerWithDrainAndDeliver T>
  void DrainAndDeliver(CommandQueue& out, JNIEnv* env,
                       const BridgeCallback& callback,
                       flatbuffers::FlatBufferBuilder& fbb);

  /// @brief Drains and delivers one handler if registered.
  /// @tparam T Handler type
  /// @param out Outgoing queue
  /// @param env JNI environment
  /// @param callback Java callback dispatcher
  /// @param fbb FlatBufferBuilder reuse buffer
  template <CommandHandlerWithDrainAndDeliver T>
  void DrainAndDeliverIfExists(CommandQueue& out, JNIEnv* env,
                               const BridgeCallback& callback,
                               flatbuffers::FlatBufferBuilder& fbb);

  /// @brief Dispatches one incoming packet to matching handler.
  /// @warning Triggers assertion if handler does not exist.
  /// @tparam T Handler type
  /// @param in Incoming queue
  /// @param pkt Parsed packet
  /// @param arena PMR memory resource
  template <CommandHandlerWithReceive T>
  void Receive(CommandQueue& in, const fbs::BridgePacket& pkt,
               std::pmr::memory_resource& arena);

  /// @brief Returns true when handler is registered.
  /// @tparam T Handler type
  /// @return `true` if handler is registered, `false` otherwise
  template <CommandHandler T>
  [[nodiscard]] bool Contains() const noexcept {
    return map_.Contains<T>();
  }

  /// @brief Checks if no handlers are registered.
  /// @return `true` if no handlers are registered, `false` otherwise
  [[nodiscard]] bool Empty() const noexcept { return map_.TypeCount() == 0; }

  /// @brief Gets the number of registered handlers.
  /// @return Number of registered handlers
  [[nodiscard]] size_t Size() const noexcept { return map_.TypeCount(); }

private:
  template <typename... Ts>
  void DrainAndDeliverAllImpl(CommandQueue& out, JNIEnv* env,
                              const BridgeCallback& callback,
                              flatbuffers::FlatBufferBuilder& fbb,
                              std::type_identity<std::tuple<Ts...>>) {
    (DrainAndDeliverIfExists<Ts>(out, env, callback, fbb), ...);
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
  auto& storage = map_.Ensure<T>();
  return {storage.template Set<T>(std::forward<Args>(args)...), true};
}

template <CommandHandler T>
inline void CommandHandlerRegistry::RemoveHandler() noexcept {
  ROSCRAFT_ASSERT(map_.Contains<T>(), "Handler '{}' does not exist!",
                  utils::TypeNameOf<T>());
  map_.Remove<T>();
}

template <CommandHandlerWithDrainAndDeliver T>
inline void CommandHandlerRegistry::DrainAndDeliver(
    CommandQueue& out, JNIEnv* env, const BridgeCallback& callback,
    flatbuffers::FlatBufferBuilder& fbb) {
  ROSCRAFT_ASSERT(map_.Contains<T>(), "Handler '{}' does not exist!",
                  utils::TypeNameOf<T>());
  auto& handler = map_.Get<T>().template Value<T>();
  handler.DrainAndDeliver(out, env, callback, fbb);
}

template <CommandHandlerWithDrainAndDeliver T>
inline void CommandHandlerRegistry::DrainAndDeliverIfExists(
    CommandQueue& out, JNIEnv* env, const BridgeCallback& callback,
    flatbuffers::FlatBufferBuilder& fbb) {
  auto* storage = map_.TryGet<T>();
  if (storage == nullptr) {
    return;
  }
  auto& handler = storage->template Value<T>();
  handler.DrainAndDeliver(out, env, callback, fbb);
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

}  // namespace jni

}  // namespace roscraft::bridge
