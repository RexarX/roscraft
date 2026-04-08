#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/command.hpp>

#include <concurrentqueue.h>

#if defined(__cpp_lib_flat_map) && __cpp_lib_flat_map >= 202207L
#include <flat_map>
#else
#include <boost/container/flat_map.hpp>
#endif

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <ranges>
#include <utility>

namespace roscraft::bridge {

namespace details {

/// @brief Base class for type-erased command storage.
class CommandStorage {
public:
  virtual ~CommandStorage() = default;

  /// @brief Clears all messages from the queue.
  virtual void Clear() = 0;

  /**
   * @brief Checks if the queue is empty.
   * @return True if no messages are stored, false otherwise
   */
  [[nodiscard]] virtual bool Empty() const noexcept = 0;

  /**
   * @brief Gets approximate number of messages stored in the queue.
   * @return Command count
   */
  [[nodiscard]] virtual size_t SizeApprox() const noexcept = 0;
};

}  // namespace details

using CommandQueueProducerToken = moodycamel::ProducerToken;
using CommandQueueConsumerToken = moodycamel::ConsumerToken;

/**
 * @brief Typed command storage backed by a lock-free concurrent queue.
 * @note Thread-safe for concurrent enqueue/dequeue operations.
 * @tparam T Command type
 */
template <CommandTrait T>
class TypedCommandStorage final : public details::CommandStorage {
public:
  TypedCommandStorage() = default;
  TypedCommandStorage(const TypedCommandStorage&) = delete;
  TypedCommandStorage(TypedCommandStorage&&) noexcept = default;
  ~TypedCommandStorage() override = default;

  TypedCommandStorage& operator=(const TypedCommandStorage&) = delete;
  TypedCommandStorage& operator=(TypedCommandStorage&&) noexcept = default;

  /// @brief Clears all messages from the queue by draining it.
  void Clear() override;

  /**
   * @brief Enqueues a single message into the queue.
   * @param message Command to enqueue
   */
  void Enqueue(const T& message)
    requires std::copy_constructible<T>
  {
    queue_.enqueue(T{message});
  }

  /**
   * @brief Enqueues a single message into the queue.
   * @param message Command to enqueue
   */
  void Enqueue(T&& message) { queue_.enqueue(std::move(message)); }

  /**
   * @brief Enqueues a single message into the queue using a producer token.
   * @param token Producer token for improved throughput
   * @param message Command to enqueue
   */
  void Enqueue(CommandQueueProducerToken& token, const T& message)
    requires std::copy_constructible<T>
  {
    queue_.enqueue(token, T{message});
  }

  /**
   * @brief Enqueues a single message into the queue using a producer token.
   * @param token Producer token for improved throughput
   * @param message Command to enqueue
   */
  void Enqueue(CommandQueueProducerToken& token, T&& message) {
    queue_.enqueue(token, std::move(message));
  }

  /**
   * @brief Enqueues multiple messages in bulk.
   * @tparam R Range type
   * @param messages Range of messages to enqueue
   */
  template <std::ranges::input_range R>
    requires std::same_as<std::ranges::range_value_t<R>, T>
  void EnqueueBulk(R&& messages);

  /**
   * @brief Enqueues multiple messages in bulk using a producer token.
   * @tparam R Range type
   * @param token Producer token for improved throughput
   * @param messages Range of messages to enqueue
   */
  template <std::ranges::input_range R>
    requires std::same_as<std::ranges::range_value_t<R>, T>
  void EnqueueBulk(CommandQueueProducerToken& token, R&& messages);

  /**
   * @brief Dequeues a single message from the queue.
   * @return Dequeued message or default-constructed `T` if the queue is empty
   */
  T Dequeue();

  /**
   * @brief Dequeues a single message from the queue.
   * @param token Consumer token for improved throughput
   * @return Dequeued message or default-constructed `T` if the queue is empty
   */
  T Dequeue(CommandQueueConsumerToken& token);

  /**
   * @brief Dequeues a single message from the queue.
   * @param dest Reference to store the dequeued message
   * @return True if an message was dequeued and stored in dest, false if the
   * queue was empty (`dest` is unchanged)
   */
  bool Dequeue(T& dest) { return queue_.try_dequeue(dest); }

  /**
   * @brief Dequeues a single message from the queue.
   * @param token Consumer token for improved throughput
   * @param dest Reference to store the dequeued message
   * @return True if an message was dequeued and stored in dest, false if the
   * queue was empty (`dest` is unchanged)
   */
  bool Dequeue(CommandQueueConsumerToken& token, T& dest) {
    return queue_.try_dequeue(token, dest);
  }

  /**
   * @brief Moves messages into an output iterator (dequeues them).
   * @tparam It Output iterator type
   * @param out Output iterator to receive messages
   * @param max_count Maximum number of messages to move (default: all messages)
   * @return Number of messages actually dequeued
   */
  template <typename It>
    requires std::output_iterator<It, T>
  size_t Into(It out, size_t max_count = std::numeric_limits<size_t>::max());

  /**
   * @brief Moves messages into an output iterator using a consumer token.
   * @tparam It Output iterator type
   * @param token Consumer token for improved throughput
   * @param out Output iterator to receive messages
   * @param max_count Maximum number of messages to move (default: all messages)
   * @return Number of messages actually dequeued
   */
  template <typename It>
    requires std::output_iterator<It, T>
  size_t Into(CommandQueueConsumerToken& token, It out,
              size_t max_count = std::numeric_limits<size_t>::max());

  /**
   * @brief Gets the producer token for this queue.
   * @return Producer token
   */
  [[nodiscard]] CommandQueueProducerToken MakeProducerToken() {
    return CommandQueueProducerToken(queue_);
  }

  /**
   * @brief Gets the consumer token for this queue.
   * @return Consumer token
   */
  [[nodiscard]] CommandQueueConsumerToken MakeConsumerToken() {
    return CommandQueueConsumerToken(queue_);
  }

  /**
   * @brief Checks if the queue is empty.
   * @return True if no messages are stored, false otherwise
   */
  [[nodiscard]] bool Empty() const noexcept override {
    return queue_.size_approx() == 0;
  }

  /**
   * @brief Gets approximate number of messages stored in the queue.
   * @return Command count
   */
  [[nodiscard]] size_t SizeApprox() const noexcept override {
    return queue_.size_approx();
  }

private:
  moodycamel::ConcurrentQueue<T> queue_;
};

template <CommandTrait T>
inline void TypedCommandStorage<T>::Clear() {
  T temp;
  while (queue_.try_dequeue(temp)) {
    // Discard messages
  }
}

template <CommandTrait T>
template <std::ranges::input_range R>
  requires std::same_as<std::ranges::range_value_t<R>, T>
inline void TypedCommandStorage<T>::EnqueueBulk(R&& messages) {
  size_t count = 0;
  if constexpr (std::ranges::sized_range<R>) {
    count = std::ranges::size(messages);
  } else {
    count = static_cast<size_t>(std::ranges::distance(messages));
  }

  if constexpr (std::ranges::contiguous_range<R>) {
    queue_.enqueue_bulk(std::ranges::data(messages), count);
  } else if constexpr (std::ranges::borrowed_range<R>) {
    queue_.enqueue_bulk(std::ranges::begin(messages), count);
  } else {
    queue_.enqueue_bulk(std::make_move_iterator(std::ranges::begin(messages)),
                        count);
  }
}

template <CommandTrait T>
template <std::ranges::input_range R>
  requires std::same_as<std::ranges::range_value_t<R>, T>
inline void TypedCommandStorage<T>::EnqueueBulk(
    CommandQueueProducerToken& token, R&& messages) {
  size_t count = 0;
  if constexpr (std::ranges::sized_range<R>) {
    count = std::ranges::size(messages);
  } else {
    count = static_cast<size_t>(std::ranges::distance(messages));
  }

  if constexpr (std::ranges::contiguous_range<R>) {
    queue_.enqueue_bulk(token, std::ranges::data(messages), count);
  } else if constexpr (std::ranges::borrowed_range<R>) {
    queue_.enqueue_bulk(token, std::ranges::begin(messages), count);
  } else {
    queue_.enqueue_bulk(
        token, std::make_move_iterator(std::ranges::begin(messages)), count);
  }
}

template <CommandTrait T>
inline T TypedCommandStorage<T>::Dequeue() {
  T result;
  queue_.try_dequeue(result);
  return result;
}

template <CommandTrait T>
inline T TypedCommandStorage<T>::Dequeue(CommandQueueConsumerToken& token) {
  T result;
  queue_.try_dequeue(token, result);
  return result;
}

template <CommandTrait T>
template <typename It>
  requires std::output_iterator<It, T>
inline size_t TypedCommandStorage<T>::Into(It out, size_t max_count) {
  size_t dequeued = 0;
  T temp;
  while (dequeued < max_count && queue_.try_dequeue(temp)) {
    *out = std::move(temp);
    ++out;
    ++dequeued;
  }
  return dequeued;
}

template <CommandTrait T>
template <typename It>
  requires std::output_iterator<It, T>
inline size_t TypedCommandStorage<T>::Into(CommandQueueConsumerToken& token,
                                           It out, size_t max_count) {
  size_t dequeued = 0;
  T temp;
  while (dequeued < max_count && queue_.try_dequeue(token, temp)) {
    *out = std::move(temp);
    ++out;
    ++dequeued;
  }
  return dequeued;
}

/**
 * @brief Async queue for managing multiple message types via lock-free
 * concurrent queues.
 * @details Each registered command type gets its own
 * `TypedCommandStorage` backed by a `moodycamel::ConcurrentQueue`.
 * @note Thread-safe.
 */
class CommandQueue {
public:
  using size_type = size_t;

  CommandQueue() = default;
  CommandQueue(const CommandQueue&) = delete;
  CommandQueue(CommandQueue&&) noexcept = default;
  ~CommandQueue() = default;

  CommandQueue& operator=(const CommandQueue&) = delete;
  CommandQueue& operator=(CommandQueue&&) noexcept = default;

  /**
   * @brief Registers command type with the queue.
   * @tparam T Command type
   */
  template <CommandTrait T>
  void Register();

  /// @brief Clears all messages from the queue maintaining all registered
  /// types.
  void Clear();

  /**
   * @brief Clears messages of a specific type.
   * @tparam T Command type
   */
  template <CommandTrait T>
  void Clear();

  /// @brief Resets the queue by clearing all messages and unregistering all
  /// message types.
  void Reset() noexcept { messages_.clear(); }

  /**
   * @brief Resets the queue by clearing and unregistering a specific message
   * type.
   * @tparam T Command type
   */
  template <CommandTrait T>
  void Reset() noexcept {
    messages_.erase(CommandTypeIndex::From<T>());
  }

  /**
   * @brief Enqueues a single message into the queue.
   * @warning Triggers assertion if message type is not registered.
   * @tparam T Command type
   * @param message Command to enqueue
   */
  template <CommandTrait T>
  void Enqueue(T&& message) {
    TypedStorage<T>().Enqueue(std::forward<T>(message));
  }

  /**
   * @brief Enqueues a single message using a producer token for improved
   * throughput.
   * @warning Triggers assertion if message type is not registered.
   * @tparam T Command type
   * @param token Producer token
   * @param message Command to enqueue
   */
  template <CommandTrait T>
  void Enqueue(CommandQueueProducerToken& token, T&& message) {
    TypedStorage<T>().Enqueue(token, std::forward<T>(message));
  }

  /**
   * @brief Enqueues multiple messages in bulk.
   * @warning Triggers assertion if message type is not registered.
   * @tparam R Range type whose value_type satisfies `CommandTrait`
   * @param messages Range of messages to enqueue
   */
  template <std::ranges::input_range R>
    requires CommandTrait<std::ranges::range_value_t<R>>
  void EnqueueBulk(R&& messages);

  /**
   * @brief Enqueues multiple messages in bulk using a producer token.
   * @warning Triggers assertion if message type is not registered.
   * @tparam R Range type whose value_type satisfies `CommandTrait`
   * @param token Producer token
   * @param messages Range of messages to enqueue
   */
  template <std::ranges::input_range R>
    requires CommandTrait<std::ranges::range_value_t<R>>
  void EnqueueBulk(CommandQueueProducerToken& token, R&& messages);

  /**
   * @brief Creates a producer token for a specific message type.
   * @warning Triggers assertion if message type is not registered.
   * @tparam T Command type
   * @return Producer token
   */
  template <CommandTrait T>
  [[nodiscard]] CommandQueueProducerToken MakeProducerToken() {
    return TypedStorage<T>().MakeProducerToken();
  }

  /**
   * @brief Creates a consumer token for a specific message type.
   * @warning Triggers assertion if message type is not registered.
   * @tparam T Command type
   * @return Consumer token
   */
  template <CommandTrait T>
  [[nodiscard]] CommandQueueConsumerToken MakeConsumerToken() {
    return TypedStorage<T>().MakeConsumerToken();
  }

  /**
   * @brief Swaps the contents of this queue with another.
   * @param other Another CommandQueue to swap with
   */
  void Swap(CommandQueue& other) noexcept {
    std::swap(messages_, other.messages_);
  }

  /**
   * @brief Swaps the contents of two CommandQueues.
   * @param lhs First CommandQueue
   * @param rhs Second CommandQueue
   */
  friend void swap(CommandQueue& lhs, CommandQueue& rhs) noexcept {
    lhs.Swap(rhs);
  }

  /**
   * @brief Checks if an message type is registered.
   * @tparam T Command type
   * @return True if the message type is registered, false otherwise
   */
  template <CommandTrait T>
  [[nodiscard]] bool IsRegistered() const noexcept {
    return messages_.contains(CommandTypeIndex::From<T>());
  }

  /**
   * @brief Checks if any registered type has messages.
   * @return True if at least one message exists across all types, false
   * otherwise
   */
  [[nodiscard]] bool HasCommands() const noexcept {
    return std::ranges::any_of(
        messages_, [](const auto& pair) { return !pair.second->Empty(); });
  }

  /**
   * @brief Checks if messages of a specific type exist in the queue.
   * @tparam T Command type
   * @return True if messages of type T exist, false otherwise or if the type is
   * not registered
   */
  template <CommandTrait T>
  [[nodiscard]] bool HasCommands() const noexcept;

  /**
   * @brief Gets the number of registered message types.
   * @return Number of distinct message types
   */
  [[nodiscard]] size_type TypeCount() const noexcept {
    return messages_.size();
  }

  /**
   * @brief Gets the approximate total number of messages across all types.
   * @return Approximate total message count
   */
  [[nodiscard]] size_type CommandCount() const noexcept;

  /**
   * @brief Gets the approximate number of messages for a specific type.
   * @tparam T Command type
   * @return Approximate message count or 0 if the type is not registered
   */
  template <CommandTrait T>
  [[nodiscard]] size_type CommandCount() const noexcept;

  /**
   * @brief Gets the typed storage for a specific message type.
   * @warning Triggers assertion if type is not registered.
   * @tparam T Command type
   * @return Reference to the typed storage
   */
  template <CommandTrait T>
  [[nodiscard]] auto TypedStorage() noexcept -> TypedCommandStorage<T>&;

  /**
   * @brief Gets the typed storage for a specific message type (const).
   * @warning Triggers assertion if type is not registered.
   * @tparam T Command type
   * @return Const reference to the typed storage
   */
  template <CommandTrait T>
  [[nodiscard]] auto TypedStorage() const noexcept
      -> const TypedCommandStorage<T>&;

private:
#if defined(__cpp_lib_flat_map) && __cpp_lib_flat_map >= 202207L
  using CommandStorage =
      std::flat_map<CommandTypeIndex, std::unique_ptr<details::CommandStorage>>;
#else
  using CommandStorage =
      boost::container::flat_map<CommandTypeIndex,
                                 std::unique_ptr<details::CommandStorage>>;
#endif

  CommandStorage messages_;  ///< Storage for commands of different types
};

template <CommandTrait T>
inline void CommandQueue::Register() {
  if (!IsRegistered<T>()) [[likely]] {
    messages_.emplace(CommandTypeIndex::From<T>(),
                      std::make_unique<TypedCommandStorage<T>>());
  }
}

inline void CommandQueue::Clear() {
  for (auto& [_, storage] : messages_) {
    storage->Clear();
  }
}

template <CommandTrait T>
inline void CommandQueue::Clear() {
  const auto it = messages_.find(CommandTypeIndex::From<T>());
  if (it == messages_.end()) [[unlikely]] {
    return;
  }
  static_cast<TypedCommandStorage<T>&>(*it->second).Clear();
}

template <std::ranges::input_range R>
  requires CommandTrait<std::ranges::range_value_t<R>>
inline void CommandQueue::EnqueueBulk(R&& messages) {
  using T = std::ranges::range_value_t<R>;
  ROSCRAFT_ASSERT(IsRegistered<T>(), "Command type '{}' is not registered!",
                  CommandNameOf<T>());
  TypedStorage<T>().EnqueueBulk(std::forward<R>(messages));
}

template <std::ranges::input_range R>
  requires CommandTrait<std::ranges::range_value_t<R>>
inline void CommandQueue::EnqueueBulk(CommandQueueProducerToken& token,
                                      R&& messages) {
  using T = std::ranges::range_value_t<R>;
  ROSCRAFT_ASSERT(IsRegistered<T>(), "Command type '{}' is not registered!",
                  CommandNameOf<T>());
  TypedStorage<T>().EnqueueBulk(token, std::forward<R>(messages));
}

template <CommandTrait T>
inline bool CommandQueue::HasCommands() const noexcept {
  const auto it = messages_.find(CommandTypeIndex::From<T>());
  if (it == messages_.end()) [[unlikely]] {
    return false;
  }
  return !static_cast<const TypedCommandStorage<T>&>(*it->second).Empty();
}

inline auto CommandQueue::CommandCount() const noexcept -> size_type {
  size_type total = 0;
  for (const auto& [_, storage] : messages_) {
    total += storage->SizeApprox();
  }
  return total;
}

template <CommandTrait T>
inline auto CommandQueue::CommandCount() const noexcept -> size_type {
  const auto it = messages_.find(CommandTypeIndex::From<T>());
  if (it == messages_.end()) [[unlikely]] {
    return 0;
  }
  return static_cast<const TypedCommandStorage<T>&>(*it->second).SizeApprox();
}

template <CommandTrait T>
inline auto CommandQueue::TypedStorage() noexcept -> TypedCommandStorage<T>& {
  ROSCRAFT_ASSERT(IsRegistered<T>(), "Command type '{}' is not registered!",
                  CommandNameOf<T>());
  return static_cast<TypedCommandStorage<T>&>(
      *messages_.at(CommandTypeIndex::From<T>()));
}

template <CommandTrait T>
inline auto CommandQueue::TypedStorage() const noexcept
    -> const TypedCommandStorage<T>& {
  ROSCRAFT_ASSERT(IsRegistered<T>(), "Command type '{}' is not registered!",
                  CommandNameOf<T>());
  return static_cast<const TypedCommandStorage<T>&>(
      *messages_.at(CommandTypeIndex::From<T>()));
}

}  // namespace roscraft::bridge
