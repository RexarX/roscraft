#pragma once

#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/command.hpp>

#if __has_include(<concurrentqueue.h>)
#include <concurrentqueue.h>
#elif __has_include(<moodycamel/concurrentqueue.h>)
#include <moodycamel/concurrentqueue.h>
#else
#error "Missing moodycamel concurrentqueue header"
#endif
#include <version>

#if defined(__cpp_lib_flat_map) && __cpp_lib_flat_map >= 202207L
#define ROSCRAFT_HAS_STD_FLAT_MAP 1
#include <flat_map>
#else
#define ROSCRAFT_HAS_STD_FLAT_MAP 0
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

  /// @brief Clears all commands from the queue.
  virtual void Clear() = 0;

  /**
   * @brief Checks if the queue is empty.
   * @return True if no commands are stored, false otherwise
   */
  [[nodiscard]] virtual bool Empty() const noexcept = 0;

  /**
   * @brief Gets approximate number of commands stored in the queue.
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

  /// @brief Clears all commands from the queue by draining it.
  void Clear() override;

  /**
   * @brief Enqueues a single command into the queue.
   * @param command Command to enqueue
   */
  void Enqueue(const T& command)
    requires std::copy_constructible<T>
  {
    queue_.enqueue(T{command});
  }

  /**
   * @brief Enqueues a single command into the queue.
   * @param command Command to enqueue
   */
  void Enqueue(T&& command) { queue_.enqueue(std::move(command)); }

  /**
   * @brief Enqueues a single command into the queue using a producer token.
   * @param token Producer token for improved throughput
   * @param command Command to enqueue
   */
  void Enqueue(CommandQueueProducerToken& token, const T& command)
    requires std::copy_constructible<T>
  {
    queue_.enqueue(token, T{command});
  }

  /**
   * @brief Enqueues a single command into the queue using a producer token.
   * @param token Producer token for improved throughput
   * @param command Command to enqueue
   */
  void Enqueue(CommandQueueProducerToken& token, T&& command) {
    queue_.enqueue(token, std::move(command));
  }

  /**
   * @brief Enqueues multiple commands in bulk.
   * @tparam R Range type
   * @param commands Range of commands to enqueue
   */
  template <std::ranges::input_range R>
    requires std::same_as<std::ranges::range_value_t<R>, T>
  void EnqueueBulk(R&& commands);

  /**
   * @brief Enqueues multiple commands in bulk using a producer token.
   * @tparam R Range type
   * @param token Producer token for improved throughput
   * @param commands Range of commands to enqueue
   */
  template <std::ranges::input_range R>
    requires std::same_as<std::ranges::range_value_t<R>, T>
  void EnqueueBulk(CommandQueueProducerToken& token, R&& commands);

  /**
   * @brief Dequeues a single command from the queue.
   * @return Dequeued command or default-constructed `T` if the queue is empty
   */
  T Dequeue();

  /**
   * @brief Dequeues a single command from the queue.
   * @param token Consumer token for improved throughput
   * @return Dequeued command or default-constructed `T` if the queue is empty
   */
  T Dequeue(CommandQueueConsumerToken& token);

  /**
   * @brief Dequeues a single command from the queue.
   * @param dest Reference to store the dequeued command
   * @return True if an command was dequeued and stored in dest, false if the
   * queue was empty (`dest` is unchanged)
   */
  bool Dequeue(T& dest) { return queue_.try_dequeue(dest); }

  /**
   * @brief Dequeues a single command from the queue.
   * @param token Consumer token for improved throughput
   * @param dest Reference to store the dequeued command
   * @return True if an command was dequeued and stored in dest, false if the
   * queue was empty (`dest` is unchanged)
   */
  bool Dequeue(CommandQueueConsumerToken& token, T& dest) {
    return queue_.try_dequeue(token, dest);
  }

  /**
   * @brief Moves commands into an output iterator (dequeues them).
   * @tparam It Output iterator type
   * @param out Output iterator to receive commands
   * @param max_count Maximum number of commands to move (default: all commands)
   * @return Number of commands actually dequeued
   */
  template <typename It>
    requires std::output_iterator<It, T>
  size_t Into(It out, size_t max_count = std::numeric_limits<size_t>::max());

  /**
   * @brief Moves commands into an output iterator using a consumer token.
   * @tparam It Output iterator type
   * @param token Consumer token for improved throughput
   * @param out Output iterator to receive commands
   * @param max_count Maximum number of commands to move (default: all commands)
   * @return Number of commands actually dequeued
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
   * @return True if no commands are stored, false otherwise
   */
  [[nodiscard]] bool Empty() const noexcept override {
    return queue_.size_approx() == 0;
  }

  /**
   * @brief Gets approximate number of commands stored in the queue.
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
    // Discard commands
  }
}

template <CommandTrait T>
template <std::ranges::input_range R>
  requires std::same_as<std::ranges::range_value_t<R>, T>
inline void TypedCommandStorage<T>::EnqueueBulk(R&& commands) {
  size_t count = 0;
  if constexpr (std::ranges::sized_range<R>) {
    count = std::ranges::size(commands);
  } else {
    count = static_cast<size_t>(std::ranges::distance(commands));
  }

  if constexpr (std::ranges::contiguous_range<R>) {
    queue_.enqueue_bulk(std::ranges::data(commands), count);
  } else if constexpr (std::ranges::borrowed_range<R>) {
    queue_.enqueue_bulk(std::ranges::begin(commands), count);
  } else {
    queue_.enqueue_bulk(std::make_move_iterator(std::ranges::begin(commands)),
                        count);
  }
}

template <CommandTrait T>
template <std::ranges::input_range R>
  requires std::same_as<std::ranges::range_value_t<R>, T>
inline void TypedCommandStorage<T>::EnqueueBulk(
    CommandQueueProducerToken& token, R&& commands) {
  size_t count = 0;
  if constexpr (std::ranges::sized_range<R>) {
    count = std::ranges::size(commands);
  } else {
    count = static_cast<size_t>(std::ranges::distance(commands));
  }

  if constexpr (std::ranges::contiguous_range<R>) {
    queue_.enqueue_bulk(token, std::ranges::data(commands), count);
  } else if constexpr (std::ranges::borrowed_range<R>) {
    queue_.enqueue_bulk(token, std::ranges::begin(commands), count);
  } else {
    queue_.enqueue_bulk(
        token, std::make_move_iterator(std::ranges::begin(commands)), count);
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
 * @brief Async queue for managing multiple command types via lock-free
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

  /// @brief Clears all commands from the queue maintaining all registered
  /// types.
  void Clear();

  /**
   * @brief Clears commands of a specific type.
   * @tparam T Command type
   */
  template <CommandTrait T>
  void Clear();

  /// @brief Resets the queue by clearing all commands and unregistering all
  /// command types.
  void Reset() noexcept { messages_.clear(); }

  /**
   * @brief Resets the queue by clearing and unregistering a specific command
   * type.
   * @tparam T Command type
   */
  template <CommandTrait T>
  void Reset() noexcept {
    messages_.erase(CommandTypeIndex::From<T>());
  }

  /**
   * @brief Enqueues a single command into the queue.
   * @warning Triggers assertion if command type is not registered.
   * @tparam T Command type
   * @param command Command to enqueue
   */
  template <CommandTrait T>
  void Enqueue(T&& command) {
    TypedStorage<T>().Enqueue(std::forward<T>(command));
  }

  /**
   * @brief Enqueues a single command using a producer token for improved
   * throughput.
   * @warning Triggers assertion if command type is not registered.
   * @tparam T Command type
   * @param token Producer token
   * @param command Command to enqueue
   */
  template <CommandTrait T>
  void Enqueue(CommandQueueProducerToken& token, T&& command) {
    TypedStorage<T>().Enqueue(token, std::forward<T>(command));
  }

  /**
   * @brief Enqueues multiple commands in bulk.
   * @warning Triggers assertion if command type is not registered.
   * @tparam R Range type whose value_type satisfies `CommandTrait`
   * @param commands Range of commands to enqueue
   */
  template <std::ranges::input_range R>
    requires CommandTrait<std::ranges::range_value_t<R>>
  void EnqueueBulk(R&& commands);

  /**
   * @brief Enqueues multiple commands in bulk using a producer token.
   * @warning Triggers assertion if command type is not registered.
   * @tparam R Range type whose value_type satisfies `CommandTrait`
   * @param token Producer token
   * @param commands Range of commands to enqueue
   */
  template <std::ranges::input_range R>
    requires CommandTrait<std::ranges::range_value_t<R>>
  void EnqueueBulk(CommandQueueProducerToken& token, R&& commands);

  /**
   * @brief Dequeues a single command from the queue.
   * @warning Triggers assertion if command type is not registered.
   * @tparam T Command type
   * @return Dequeued command or default-constructed `T` if the queue is empty
   */
  template <CommandTrait T>
  T Dequeue();

  /**
   * @brief Dequeues a single command from the queue.
   * @warning Triggers assertion if command type is not registered.
   * @tparam T Command type
   * @param token Consumer token for improved throughput
   * @return Dequeued command or default-constructed `T` if the queue is empty
   */
  template <CommandTrait T>
  T Dequeue(CommandQueueConsumerToken& token);

  /**
   * @brief Dequeues a single command from the queue.
   * @warning Triggers assertion if command type is not registered.
   * @tparam T Command type
   * @param dest Reference to store the dequeued command
   * @return True if an command was dequeued and stored in dest, false if the
   * queue was empty (`dest` is unchanged)
   */
  template <CommandTrait T>
  bool Dequeue(T& dest);

  /**
   * @brief Dequeues a single command from the queue.
   * @warning Triggers assertion if command type is not registered.
   * @tparam T Command type
   * @param token Consumer token for improved throughput
   * @param dest Reference to store the dequeued command
   * @return True if an command was dequeued and stored in dest, false if the
   * queue was empty (`dest` is unchanged)
   */
  template <CommandTrait T>
  bool Dequeue(CommandQueueConsumerToken& token, T& dest);

  /**
   * @brief Moves commands into an output iterator (dequeues them).
   * @warning Triggers assertion if command type is not registered.
   * @tparam It Output iterator type
   * @param out Output iterator to receive commands
   * @param max_count Maximum number of commands to move (default: all commands)
   * @return Number of commands actually dequeued
   */
  template <typename It>
  size_t Into(It out, size_t max_count = std::numeric_limits<size_t>::max());

  /**
   * @brief Moves commands into an output iterator using a consumer token.
   * @warning Triggers assertion if command type is not registered.
   * @tparam It Output iterator type
   * @param token Consumer token for improved throughput
   * @param out Output iterator to receive commands
   * @param max_count Maximum number of commands to move (default: all commands)
   * @return Number of commands actually dequeued
   */
  template <typename It>
  size_t Into(CommandQueueConsumerToken& token, It out,
              size_t max_count = std::numeric_limits<size_t>::max());

  /**
   * @brief Creates a producer token for a specific command type.
   * @warning Triggers assertion if command type is not registered.
   * @tparam T Command type
   * @return Producer token
   */
  template <CommandTrait T>
  [[nodiscard]] CommandQueueProducerToken MakeProducerToken() {
    return TypedStorage<T>().MakeProducerToken();
  }

  /**
   * @brief Creates a consumer token for a specific command type.
   * @warning Triggers assertion if command type is not registered.
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
   * @brief Checks if an command type is registered.
   * @tparam T Command type
   * @return True if the command type is registered, false otherwise
   */
  template <CommandTrait T>
  [[nodiscard]] bool IsRegistered() const noexcept {
    return messages_.contains(CommandTypeIndex::From<T>());
  }

  /**
   * @brief Checks if any registered type has commands.
   * @return True if at least one command exists across all types, false
   * otherwise
   */
  [[nodiscard]] bool HasCommands() const noexcept {
    return std::ranges::any_of(
        messages_, [](const auto& pair) { return !pair.second->Empty(); });
  }

  /**
   * @brief Checks if commands of a specific type exist in the queue.
   * @tparam T Command type
   * @return True if commands of type T exist, false otherwise or if the type is
   * not registered
   */
  template <CommandTrait T>
  [[nodiscard]] bool HasCommands() const noexcept;

  /**
   * @brief Gets the number of registered command types.
   * @return Number of distinct command types
   */
  [[nodiscard]] size_type TypeCount() const noexcept {
    return messages_.size();
  }

  /**
   * @brief Gets the approximate total number of commands across all types.
   * @return Approximate total command count
   */
  [[nodiscard]] size_type CommandCount() const noexcept;

  /**
   * @brief Gets the approximate number of commands for a specific type.
   * @tparam T Command type
   * @return Approximate command count or 0 if the type is not registered
   */
  template <CommandTrait T>
  [[nodiscard]] size_type CommandCount() const noexcept;

  /**
   * @brief Gets the typed storage for a specific command type.
   * @warning Triggers assertion if type is not registered.
   * @tparam T Command type
   * @return Reference to the typed storage
   */
  template <CommandTrait T>
  [[nodiscard]] auto TypedStorage() noexcept -> TypedCommandStorage<T>&;

  /**
   * @brief Gets the typed storage for a specific command type (const).
   * @warning Triggers assertion if type is not registered.
   * @tparam T Command type
   * @return Const reference to the typed storage
   */
  template <CommandTrait T>
  [[nodiscard]] auto TypedStorage() const noexcept
      -> const TypedCommandStorage<T>&;

private:
#if ROSCRAFT_HAS_STD_FLAT_MAP
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
  for (auto&& [_, storage] : messages_) {
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
inline void CommandQueue::EnqueueBulk(R&& commands) {
  using T = std::ranges::range_value_t<R>;
  ROSCRAFT_ASSERT(IsRegistered<T>(), "Command type '{}' is not registered!",
                  CommandNameOf<T>());
  TypedStorage<T>().EnqueueBulk(std::forward<R>(commands));
}

template <std::ranges::input_range R>
  requires CommandTrait<std::ranges::range_value_t<R>>
inline void CommandQueue::EnqueueBulk(CommandQueueProducerToken& token,
                                      R&& commands) {
  using T = std::ranges::range_value_t<R>;
  ROSCRAFT_ASSERT(IsRegistered<T>(), "Command type '{}' is not registered!",
                  CommandNameOf<T>());
  TypedStorage<T>().EnqueueBulk(token, std::forward<R>(commands));
}

template <CommandTrait T>
inline T CommandQueue::Dequeue() {
  ROSCRAFT_ASSERT(IsRegistered<T>(), "Command type '{}' is not registered!",
                  CommandNameOf<T>());
  return TypedStorage<T>().Dequeue();
}

template <CommandTrait T>
inline T CommandQueue::Dequeue(CommandQueueConsumerToken& token) {
  ROSCRAFT_ASSERT(IsRegistered<T>(), "Command type '{}' is not registered!",
                  CommandNameOf<T>());
  return TypedStorage<T>().Dequeue(token);
}

template <CommandTrait T>
inline bool CommandQueue::Dequeue(T& dest) {
  ROSCRAFT_ASSERT(IsRegistered<T>(), "Command type '{}' is not registered!",
                  CommandNameOf<T>());
  return TypedStorage<T>().Dequeue(dest);
}

template <CommandTrait T>
inline bool CommandQueue::Dequeue(CommandQueueConsumerToken& token, T& dest) {
  ROSCRAFT_ASSERT(IsRegistered<T>(), "Command type '{}' is not registered!",
                  CommandNameOf<T>());
  return TypedStorage<T>().Dequeue(token, dest);
}

template <typename It>
inline size_t CommandQueue::Into(It out, size_t max_count) {
  using T = std::iter_value_t<It>;
  ROSCRAFT_ASSERT(IsRegistered<T>(), "Command type '{}' is not registered!",
                  CommandNameOf<T>());
  return TypedStorage<T>().Into(std::move(out), max_count);
}

template <typename It>
inline size_t CommandQueue::Into(CommandQueueConsumerToken& token, It out,
                                 size_t max_count) {
  using T = std::iter_value_t<It>;
  ROSCRAFT_ASSERT(IsRegistered<T>(), "Command type '{}' is not registered!",
                  CommandNameOf<T>());
  return TypedStorage<T>().Into(token, std::move(out), max_count);
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
