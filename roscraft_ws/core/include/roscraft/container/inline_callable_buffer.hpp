#pragma once

#include <roscraft/utils/common_traits.hpp>

#include <concepts>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <memory_resource>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace roscraft::container {

namespace details {

/// @brief Concept for void(Args...) function signatures.
template <typename Sig>
concept VoidSignature = requires {
  []<typename... Args>(void (*)(Args...)) {}(static_cast<Sig*>(nullptr));
};

/// @brief Extracts argument types from a void(Args...) signature.
template <typename Sig>
struct SignatureArgsHelper;

template <typename... Args>
struct SignatureArgsHelper<void(Args...)> {
  using Type = std::tuple<Args...>;
  static constexpr size_t kCount = sizeof...(Args);
};

template <typename Sig>
using SignatureArgsT = typename SignatureArgsHelper<Sig>::Type;

/// @brief Gets the Nth signature from a pack of signatures.
template <size_t N, typename First, typename... Rest>
struct NthSignatureHelper {
  using Type = typename NthSignatureHelper<N - 1, Rest...>::Type;
};

template <typename First, typename... Rest>
struct NthSignatureHelper<0, First, Rest...> {
  using Type = First;
};

template <size_t N, typename... Sigs>
  requires(N < sizeof...(Sigs))
using NthSignatureT = typename NthSignatureHelper<N, Sigs...>::Type;

/// @brief Gets the argument tuple for the Nth signature.
template <size_t N, typename... Sigs>
using NthSignatureArgsT = SignatureArgsT<NthSignatureT<N, Sigs...>>;

/// @brief Converts a tuple of argument types to a function pointer type.
template <typename ArgsTuple>
struct TupleToFunctionPtrHelper;

template <typename... Args>
struct TupleToFunctionPtrHelper<std::tuple<Args...>> {
  using Type = void (*)(Args..., void*);
};

template <typename ArgsTuple>
using TupleToFunctionPtrType =
    typename TupleToFunctionPtrHelper<ArgsTuple>::Type;

/// @brief Gets the Nth element from a non-type template parameter pack.
template <size_t N, auto First, auto... Rest>
struct NthMethodHelper {
  static constexpr auto kValue = NthMethodHelper<N - 1, Rest...>::kValue;
};

template <auto First, auto... Rest>
struct NthMethodHelper<0, First, Rest...> {
  static constexpr auto kValue = First;
};

template <size_t N, auto... Methods>
  requires(N < sizeof...(Methods))
inline constexpr auto kNthMethod = NthMethodHelper<N, Methods...>::kValue;

/// @brief Concept checking if UArgs are polymorphically convertible to the
/// signature's args.
template <typename ArgsTuple, typename... UArgs>
concept ArgsConvertibleTo = []<typename... SigArgs>(std::tuple<SigArgs...>*) {
  return (sizeof...(SigArgs) == sizeof...(UArgs)) &&
         (sizeof...(SigArgs) == 0 ||
          (utils::PolymorphicConvertible<UArgs, SigArgs> && ...));
}(static_cast<ArgsTuple*>(nullptr));

/**
 * @brief Concept: T is directly invocable with signature arguments and returns
 * void.
 * @details Used for single-signature buffers where operator()(Args...) is
 * called.
 */
template <typename T, typename ArgsTuple>
concept DirectlyInvocableWith = []<typename... Args>(std::tuple<Args...>*) {
  using DecayedT = std::remove_cvref_t<T>;
  return std::invocable<DecayedT&, Args...> &&
         std::is_void_v<std::invoke_result_t<DecayedT&, Args...>>;
}(static_cast<ArgsTuple*>(nullptr));

/**
 * @brief Concept: T is invocable with an index constant and signature
 * arguments, returning void.
 * @details Used for multi-signature buffers where
 * operator()(integral_constant<N>, Args...) is called.
 */
template <typename T, size_t N, typename ArgsTuple>
concept IndexedInvocableWith = []<typename... Args>(std::tuple<Args...>*) {
  using DecayedT = std::remove_cvref_t<T>;
  return std::invocable<DecayedT&, std::integral_constant<size_t, N>,
                        Args...> &&
         std::is_void_v<std::invoke_result_t<
             DecayedT&, std::integral_constant<size_t, N>, Args...>>;
}(static_cast<ArgsTuple*>(nullptr));

/**
 * @brief Concept: T is invocable with all signatures using indexed dispatch.
 * @details Verifies operator()(integral_constant<N>, Args...) for each
 * signature index N.
 */
template <typename T, typename... Signatures>
concept AllIndexedInvocable = []<size_t... Indices>(
                                  std::index_sequence<Indices...>) {
  return (IndexedInvocableWith<T, Indices, SignatureArgsT<Signatures>> && ...);
}(std::make_index_sequence<sizeof...(Signatures)>{});

/**
 * @brief Concept for callables usable with single-signature default Push.
 * @details Callable must have operator()(Args...) returning void.
 */
template <typename T, typename Signature>
concept SingleSignatureCallable =
    DirectlyInvocableWith<T, SignatureArgsT<Signature>>;

/**
 * @brief Concept for callables usable with multi-signature default Push.
 * @details Callable must have operator()(integral_constant<N>, Args...) for
 * each signature N.
 */
template <typename T, typename... Signatures>
concept MultiSignatureCallable =
    (sizeof...(Signatures) > 1) && AllIndexedInvocable<T, Signatures...>;

/**
 * @brief Helper struct for DefaultPushCallable concept with variadic
 * signatures.
 */
template <typename T, typename... Sigs>
struct DefaultPushCallableHelper;

template <typename T, typename FirstSig>
struct DefaultPushCallableHelper<T, FirstSig> {
  static constexpr bool kValue = SingleSignatureCallable<T, FirstSig>;
};

template <typename T, typename FirstSig, typename... RestSigs>
struct DefaultPushCallableHelper<T, FirstSig, RestSigs...> {
  static constexpr bool kValue =
      MultiSignatureCallable<T, FirstSig, RestSigs...>;
};

/**
 * @brief Concept for default Push callables.
 * @details Combines single and multi-signature requirements.
 */
template <typename T, typename... Sigs>
concept DefaultPushCallable =
    (sizeof...(Sigs) > 0) && DefaultPushCallableHelper<T, Sigs...>::kValue;

/// @brief Concept: a method is invocable on T with signature arguments and
/// returns void.
template <auto Method, typename T, typename ArgsTuple>
concept MethodInvocableWith = []<typename... Args>(std::tuple<Args...>*) {
  using DecayedT = std::remove_cvref_t<T>;
  return std::invocable<decltype(Method), DecayedT&, Args...> &&
         std::is_void_v<
             std::invoke_result_t<decltype(Method), DecayedT&, Args...>>;
}(static_cast<ArgsTuple*>(nullptr));

/// @brief Helper to validate all methods match their corresponding signatures.
template <typename T, typename SignatureTuple, auto... Methods>
struct AllMethodsValidHelper;

template <typename T, typename... Signatures, auto... Methods>
struct AllMethodsValidHelper<T, std::tuple<Signatures...>, Methods...> {
  static constexpr bool kValue = []<size_t... Indices>(
                                     std::index_sequence<Indices...>) {
    return (MethodInvocableWith<kNthMethod<Indices, Methods...>, T,
                                NthSignatureArgsT<Indices, Signatures...>> &&
            ...);
  }(std::make_index_sequence<sizeof...(Methods)>{});
};

/// @brief Concept: all methods are valid for their corresponding signatures.
template <typename T, typename SignatureTuple, auto... Methods>
concept AllMethodsValid =
    AllMethodsValidHelper<T, SignatureTuple, Methods...>::kValue;

}  // namespace details

/**
 * @brief Concept for types storable in InlineCallableBuffer.
 * @details Types must be destructible, copy or move constructible, not
 * references, not polymorphic, not abstract, and have alignment not exceeding
 * `std::max_align_t`.
 */
template <typename T>
concept InlineCallableBufferStorable =
    std::destructible<T> &&
    (std::move_constructible<T> || std::copy_constructible<T>) &&
    !std::is_polymorphic_v<T> && !std::is_abstract_v<T> &&
    (alignof(std::remove_cvref_t<T>) <= alignof(std::max_align_t));

/**
 * @brief Implementation class for inline callable buffer with explicit
 * allocator.
 * @details This is the actual implementation. Use `InlineCallableBuffer` alias
 * for ergonomic usage.
 * @tparam Allocator Allocator type for the internal byte buffer (default:
 * `std::allocator<std::byte>`).
 * @tparam Signatures Function signatures in the form void(Args...).
 */
template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
class InlineCallableBufferImpl {
public:
  static constexpr size_t kNumOperations = sizeof...(Signatures);

  using allocator_type = Allocator;
  using byte_allocator_type = typename std::allocator_traits<
      allocator_type>::template rebind_alloc<std::byte>;
  using offset_allocator_type = typename std::allocator_traits<
      allocator_type>::template rebind_alloc<size_t>;

  using size_type = size_t;

private:
  using DestroyFn = void (*)(void*);
  using RelocateFn = void (*)(void* dest, void* src);

  /// @brief Function pointer type for the first signature (used for size
  /// calculations).
  using FirstExecuteFn = details::TupleToFunctionPtrType<
      details::NthSignatureArgsT<0, Signatures...>>;

  // All ExecuteFn types are regular function pointers, so they should have the
  // same size.
  static_assert(((sizeof(details::TupleToFunctionPtrType<
                         details::NthSignatureArgsT<0, Signatures...>>) ==
                  sizeof(details::TupleToFunctionPtrType<
                         details::SignatureArgsT<Signatures>>)) &&
                 ...),
                "All function pointer types must have the same size");

  using BufferType = std::vector<std::byte, byte_allocator_type>;
  using OffsetsType = std::vector<size_type, offset_allocator_type>;

  /// @brief Pack of signature types for use in method validation.
  using SignaturePack = std::tuple<Signatures...>;

public:
  InlineCallableBufferImpl() = default;

  /**
   * @brief Constructs with a custom allocator.
   * @param alloc Allocator instance to use
   */
  explicit InlineCallableBufferImpl(const allocator_type& alloc)
      : buffer_(byte_allocator_type(alloc)),
        offsets_(offset_allocator_type(alloc)) {}

  /**
   * @brief Constructs with a PMR memory resource.
   * @details Enabled only when `allocator_type` is constructible from
   * `std::pmr::memory_resource*`.
   * @param resource Memory resource used to construct allocator
   */
  explicit InlineCallableBufferImpl(std::pmr::memory_resource* resource)
    requires std::constructible_from<allocator_type, std::pmr::memory_resource*>
      : InlineCallableBufferImpl(allocator_type{resource}) {}

  InlineCallableBufferImpl(const InlineCallableBufferImpl&) = delete;
  InlineCallableBufferImpl(InlineCallableBufferImpl&& other) noexcept
      : buffer_(std::move(other.buffer_)),
        offsets_(std::move(other.offsets_)) {}

  ~InlineCallableBufferImpl() noexcept { Clear(); }

  InlineCallableBufferImpl& operator=(const InlineCallableBufferImpl&) = delete;
  InlineCallableBufferImpl& operator=(
      InlineCallableBufferImpl&& other) noexcept;

  /// @brief Clears all stored callables, calling destructors as needed.
  void Clear() noexcept;

  /**
   * @brief Push a callable with default `operator()` for invocation.
   * @details For single-signature buffers, uses `operator()(Args...)`.
   * For multi-signature buffers, uses
   * `operator()(std::integral_constant<size_t, N>, Args...)`.
   * @tparam T Callable type that must be invocable with the appropriate
   * signature(s) and return void.
   * @param callable Callable object to store
   */
  template <InlineCallableBufferStorable T>
    requires details::DefaultPushCallable<std::remove_cvref_t<T>, Signatures...>
  void Push(T&& callable) {
    PushImpl(std::forward<T>(callable));
  }

  /**
   * @brief Push a callable with custom member function pointers.
   * @details Allows specifying custom function names for each operation.
   * Number of Methods must match `kNumOperations`.
   * Each method must be invocable on `T` with the corresponding signature's
   * arguments and return void.
   * @tparam Methods Member function pointers for each operation
   * @tparam T Callable type (deduced)
   * @param callable Callable object to store
   *
   * @example
   * @code
   * // Single operation
   * buffer.Push<&MyCmd::Run>(MyCmd{});
   *
   * // Multiple operations
   * buffer.Push<&MyCmd::Execute, &MyCmd::Log>(MyCmd{});
   * @endcode
   */
  template <auto... Methods, InlineCallableBufferStorable T>
    requires details::AllMethodsValid<T, SignaturePack, Methods...> &&
             (sizeof...(Methods) == kNumOperations)
  void Push(T&& callable) {
    PushImplMethods<Methods...>(std::forward<T>(callable));
  }

  /**
   * @brief Invoke operation `N` on all stored callables in insertion order.
   * @details Supports polymorphic argument conversion for flexible invocation.
   * @tparam N Operation index (0 to `kNumOperations-1`)
   * @tparam UArgs Actual argument types (must be convertible to signature args)
   * @param args Arguments for operation `N`
   */
  template <size_t N = 0, typename... UArgs>
    requires details::ArgsConvertibleTo<
                 details::NthSignatureArgsT<N, Signatures...>, UArgs...> &&
             (N < sizeof...(Signatures))
  void Invoke(UArgs&&... args) noexcept;

  /**
   * @brief Reserves bytes in the internal buffer.
   * @param bytes Number of bytes to reserve
   */
  void ReserveBytes(size_type bytes) { buffer_.reserve(bytes); }

  /**
   * @brief Reserves space for approximately count callables.
   * @details Estimates average callable size for reservation.
   * @param count Approximate number of callables
   */
  void Reserve(size_type count);

  /**
   * @brief Swaps contents with another buffer.
   * @param other Buffer to swap with
   */
  void Swap(InlineCallableBufferImpl& other) noexcept(
      std::is_nothrow_swappable_v<BufferType> &&
      std::is_nothrow_swappable_v<OffsetsType>);

  friend void swap(InlineCallableBufferImpl& lhs,
                   InlineCallableBufferImpl&
                       rhs) noexcept(std::is_nothrow_swappable_v<BufferType> &&
                                     std::is_nothrow_swappable_v<OffsetsType>) {
    lhs.Swap(rhs);
  }

  /**
   * @brief Checks if the buffer is empty.
   * @return true if empty, false otherwise
   */
  [[nodiscard]] bool Empty() const noexcept { return offsets_.empty(); }

  /**
   * @brief Gets the number of stored callables.
   * @return Number of callables
   */
  [[nodiscard]] size_type Size() const noexcept { return offsets_.size(); }

  /**
   * @brief Gets the current capacity in bytes of the internal buffer.
   * @return Capacity in bytes
   */
  [[nodiscard]] size_type CapacityBytes() const noexcept {
    return buffer_.capacity();
  }

  /**
   * @brief Gets the allocator used by the buffer.
   * @return Allocator instance
   */
  [[nodiscard]] allocator_type GetAllocator() const noexcept {
    return allocator_type(buffer_.get_allocator());
  }

private:
  /**
   * @brief Header layout per callable entry:
   *   - kNumOperations function pointers (one per signature)
   *   - DestroyFn pointer (or `nullptr` if trivially destructible)
   *   - RelocateFn pointer (or `nullptr` if trivially copyable)
   *   - size_type data_offset (offset from header start to callable data)
   *   - [padding for alignment]
   *   - Callable data
   */
  static constexpr size_type BaseHeaderSize() noexcept {
    return (kNumOperations * sizeof(FirstExecuteFn)) + sizeof(DestroyFn) +
           sizeof(RelocateFn) + sizeof(size_type);
  }

  static constexpr size_type AlignUp(size_type offset,
                                     size_type alignment) noexcept {
    return (offset + alignment - 1) & ~(alignment - 1);
  }

  template <typename T, size_t N, typename... Args>
  static void ExecuteDefault(Args... args, void* data) noexcept;

  template <typename T, auto Method, typename... Args>
  static void ExecuteMethod(Args... args, void* data) noexcept;

  template <typename T>
  static void DestroyCallable(void* data) noexcept;

  template <typename T>
  static void RelocateCallable(void* dest, void* src) noexcept;

  void GrowBuffer(size_type required_capacity);

  template <typename T>
  void PushImpl(T&& callable);

  template <auto... Methods, typename T>
  void PushImplMethods(T&& callable);

  template <typename T, size_t... Indices>
  void StoreFunctionPointersDefault(
      size_type header_offset, std::index_sequence<Indices...> seq) noexcept;

  template <typename T, auto... Methods, size_t... Indices>
  void StoreFunctionPointersMethods(
      size_type header_offset, std::index_sequence<Indices...> seq) noexcept;

  [[nodiscard]] void* GetDataPtr(size_type header_offset) const noexcept;

  BufferType buffer_;
  OffsetsType offsets_;
};

namespace details {

/// @brief Concept for types that are instantiations of a class template (i.e.,
/// allocator-like).
/// @details Checks whether `T` supports `std::allocator_traits` rebinding,
/// which all standard-conforming allocators do. Guards against function types
/// and void-signature types to avoid hard errors when evaluated during template
/// deduction.
template <typename T>
concept InstantiatedAllocator =
    std::is_class_v<T> && !VoidSignature<T> && requires {
      typename std::allocator_traits<T>::template rebind_alloc<std::byte>;
    };

/// @brief Deduces the `InlineCallableBufferImpl` type from signature arguments.
/// @details If the first argument is a `void(Args...)` signature, all arguments
/// are treated as signatures and the default allocator
/// (`std::allocator<std::byte>`) is used. If the first argument is an
/// instantiated allocator type (e.g., `std::allocator<std::byte>`), it is used
/// directly as the allocator and the remaining arguments are treated as
/// signatures.
template <typename... Args>
struct InlineCallableBufferDeducer;

// All arguments are void(Args...) signatures - use default allocator
template <VoidSignature FirstSig, typename... RestSigs>
  requires(VoidSignature<RestSigs> && ...)
struct InlineCallableBufferDeducer<FirstSig, RestSigs...> {
  using type = InlineCallableBufferImpl<std::allocator<std::byte>, FirstSig,
                                        RestSigs...>;
};

// First argument is an instantiated allocator type, remaining are signatures
template <typename Alloc, VoidSignature FirstSig, typename... RestSigs>
  requires InstantiatedAllocator<Alloc> && (VoidSignature<RestSigs> && ...)
struct InlineCallableBufferDeducer<Alloc, FirstSig, RestSigs...> {
  using type = InlineCallableBufferImpl<Alloc, FirstSig, RestSigs...>;
};

}  // namespace details

/**
 * @brief Inline storage for heterogeneous callable instances with type-erased
 * invocation.
 * @details Stores callable instances of different types in a single contiguous
 * buffer with embedded function pointers for type-safe invocation. Optimized
 * for fast sequential iteration without virtual dispatch or per-callable heap
 * allocations.
 *
 * Unlike `TypedBuffer` (homogeneous), this container preserves insertion order
 * across different callable types while maintaining cache-friendly layout by
 * storing instances inline.
 *
 * Each callable instance can use custom member function names by specifying
 * them as template parameters when pushing.
 *
 * The allocator parameter is optional.
 * If the first template argument is a function signature (`void(Args...)`), the
 * default allocator is used. Otherwise, the first argument is treated as an
 * allocator.
 *
 * @tparam Args Either signatures only, or allocator followed by signatures
 *
 * @example
 * @code
 * // Single operation with default allocator (most common usage)
 * InlineCallableBuffer<void(World&)> commands;
 * commands.Push(SpawnEntityCmd{entity});
 * commands.Invoke(world);
 *
 * // Multiple operations with default allocator
 * InlineCallableBuffer<void(World&), void(Logger&)> multi_commands;
 * multi_commands.Push<&Cmd::Execute, &Cmd::Log>(Cmd{data});
 * multi_commands.Invoke<0>(world);
 * multi_commands.Invoke<1>(logger);
 *
 * // With custom allocator (pass an instantiated allocator type)
 * InlineCallableBuffer<MyAllocator<std::byte>, void(World&)>
 * custom_commands{my_allocator};
 * @endcode
 */
template <typename... Args>
using InlineCallableBuffer =
    typename details::InlineCallableBufferDeducer<Args...>::type;

template <typename... Signatures>
using PmrInlineCallableBuffer =
    InlineCallableBufferImpl<std::pmr::polymorphic_allocator<std::byte>,
                             Signatures...>;

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
inline auto InlineCallableBufferImpl<Allocator, Signatures...>::operator=(
    InlineCallableBufferImpl&& other) noexcept -> InlineCallableBufferImpl& {
  if (this == &other) [[unlikely]] {
    return *this;
  }

  Clear();
  buffer_ = std::move(other.buffer_);
  offsets_ = std::move(other.offsets_);

  return *this;
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
inline void
InlineCallableBufferImpl<Allocator, Signatures...>::Clear() noexcept {
  // Destroy in reverse order to maintain LIFO semantics
  for (auto it = offsets_.rbegin(); it != offsets_.rend(); ++it) {
    auto header_offset = *it;
    auto* header_base = buffer_.data() + header_offset;

    auto* destroy_fn_ptr = reinterpret_cast<DestroyFn*>(
        header_base + (kNumOperations * sizeof(FirstExecuteFn)));
    auto destroy_fn = *destroy_fn_ptr;

    if (destroy_fn != nullptr) {
      auto* data = GetDataPtr(header_offset);
      destroy_fn(data);
    }
  }

  buffer_.clear();
  offsets_.clear();
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
template <size_t N, typename... UArgs>
  requires details::ArgsConvertibleTo<
               details::NthSignatureArgsT<N, Signatures...>, UArgs...> &&
           (N < sizeof...(Signatures))
inline void InlineCallableBufferImpl<Allocator, Signatures...>::Invoke(
    UArgs&&... args) noexcept {
  using ArgsTuple = details::NthSignatureArgsT<N, Signatures...>;
  using ExecuteFn = details::TupleToFunctionPtrType<ArgsTuple>;

  for (size_type header_offset : offsets_) {
    auto* header_base = buffer_.data() + header_offset;
    auto* exec_fn_ptr = reinterpret_cast<ExecuteFn*>(
        header_base + (N * sizeof(FirstExecuteFn)));
    auto exec_fn = *exec_fn_ptr;
    auto* data = GetDataPtr(header_offset);
    exec_fn(std::forward<UArgs>(args)..., data);
  }
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
inline void InlineCallableBufferImpl<Allocator, Signatures...>::Reserve(
    size_type count) {
  offsets_.reserve(count);
  // Estimate average callable size as 32 bytes plus header
  buffer_.reserve(count * (BaseHeaderSize() + 32));
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
inline void InlineCallableBufferImpl<Allocator, Signatures...>::Swap(
    InlineCallableBufferImpl&
        other) noexcept(std::is_nothrow_swappable_v<BufferType> &&
                        std::is_nothrow_swappable_v<OffsetsType>) {
  buffer_.swap(other.buffer_);
  offsets_.swap(other.offsets_);
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
template <typename T, size_t N, typename... Args>
inline void InlineCallableBufferImpl<Allocator, Signatures...>::ExecuteDefault(
    Args... args, void* data) noexcept {
  T* callable = static_cast<T*>(data);

  if constexpr (kNumOperations == 1) {
    // Single operation: use operator()(Args...)
    std::invoke(*callable, args...);
  } else {
    // Multiple operations: use operator()(std::integral_constant<size_t, N>,
    // Args...)
    std::invoke(*callable, std::integral_constant<size_t, N>{}, args...);
  }
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
template <typename T, auto Method, typename... Args>
inline void InlineCallableBufferImpl<Allocator, Signatures...>::ExecuteMethod(
    Args... args, void* data) noexcept {
  T* callable = static_cast<T*>(data);
  std::invoke(Method, callable, args...);
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
template <typename T>
inline void InlineCallableBufferImpl<Allocator, Signatures...>::DestroyCallable(
    void* data) noexcept {
  T* callable = static_cast<T*>(data);
  std::destroy_at(callable);
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
template <typename T>
inline void
InlineCallableBufferImpl<Allocator, Signatures...>::RelocateCallable(
    void* dest, void* src) noexcept {
  T* typed_src = static_cast<T*>(src);
  std::construct_at(static_cast<T*>(dest), std::move(*typed_src));
  std::destroy_at(typed_src);
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
inline void InlineCallableBufferImpl<Allocator, Signatures...>::GrowBuffer(
    size_type required_capacity) {
  constexpr size_type fn_ptr_size = sizeof(FirstExecuteFn);

  BufferType new_buffer(buffer_.get_allocator());
  auto new_cap = std::max(required_capacity, buffer_.capacity() * 2);
  new_buffer.resize(new_cap);

  // Copy all header metadata (trivial bytes) to the new buffer
  if (!buffer_.empty()) {
    std::memcpy(new_buffer.data(), buffer_.data(), buffer_.size());
  }

  // Relocate non-trivially-copyable callable data using stored RelocateFn
  for (auto header_offset : offsets_) {
    auto* old_header = buffer_.data() + header_offset;
    auto* new_header = new_buffer.data() + header_offset;

    auto* relocate_fn_ptr = reinterpret_cast<RelocateFn*>(
        old_header + (kNumOperations * fn_ptr_size) + sizeof(DestroyFn));
    auto relocate_fn = *relocate_fn_ptr;

    if (relocate_fn != nullptr) {
      auto* data_offset_ptr = reinterpret_cast<size_type*>(
          old_header + (kNumOperations * fn_ptr_size) + sizeof(DestroyFn) +
          sizeof(RelocateFn));
      auto data_offset = *data_offset_ptr;

      auto* old_data = old_header + data_offset;
      auto* new_data = new_header + data_offset;

      relocate_fn(new_data, old_data);
    }
  }

  // Trim new_buffer to actual used size
  auto used = buffer_.size();
  buffer_ = std::move(new_buffer);
  buffer_.resize(used);
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
template <typename T>
inline void InlineCallableBufferImpl<Allocator, Signatures...>::PushImpl(
    T&& callable) {
  using DecayedT = std::remove_cvref_t<T>;

  constexpr size_type element_size = sizeof(DecayedT);
  constexpr size_type element_align = alignof(DecayedT);
  constexpr size_type base_header = BaseHeaderSize();
  constexpr size_type fn_ptr_size = sizeof(FirstExecuteFn);

  // Align header to function pointer alignment
  auto header_offset = AlignUp(buffer_.size(), alignof(FirstExecuteFn));

  // Calculate data offset using relative offsets (not absolute addresses)
  // This ensures correctness even if the buffer reallocates to a different
  // address
  auto unaligned_data_offset = header_offset + base_header;
  auto aligned_data_offset = AlignUp(unaligned_data_offset, element_align);
  auto data_offset_from_header = aligned_data_offset - header_offset;
  auto total_entry_size = data_offset_from_header + element_size;

  auto required_size = header_offset + total_entry_size;

  // Grow the buffer properly if reallocation would be needed
  if (required_size > buffer_.capacity()) {
    GrowBuffer(required_size);
  }

  // Resize to final size (no reallocation since we ensured capacity above)
  buffer_.resize(required_size);

  auto* header_base = buffer_.data() + header_offset;

  // Store function pointers for each operation
  StoreFunctionPointersDefault<DecayedT>(
      header_offset, std::make_index_sequence<kNumOperations>{});

  // Store destroy function pointer
  auto* destroy_fn_ptr = reinterpret_cast<DestroyFn*>(
      header_base + (kNumOperations * fn_ptr_size));
  if constexpr (std::is_trivially_destructible_v<DecayedT>) {
    *destroy_fn_ptr = nullptr;
  } else {
    *destroy_fn_ptr = &DestroyCallable<DecayedT>;
  }

  // Store relocate function pointer
  auto* relocate_fn_ptr = reinterpret_cast<RelocateFn*>(
      header_base + (kNumOperations * fn_ptr_size) + sizeof(DestroyFn));
  if constexpr (std::is_trivially_copyable_v<DecayedT>) {
    *relocate_fn_ptr = nullptr;
  } else {
    *relocate_fn_ptr = &RelocateCallable<DecayedT>;
  }

  // Store data offset (relative to header start)
  auto* data_offset_storage = reinterpret_cast<size_type*>(
      header_base + (kNumOperations * fn_ptr_size) + sizeof(DestroyFn) +
      sizeof(RelocateFn));
  *data_offset_storage = data_offset_from_header;

  // Construct the callable in-place
  auto* data = header_base + data_offset_from_header;
  std::construct_at(reinterpret_cast<DecayedT*>(data),
                    std::forward<T>(callable));

  offsets_.push_back(header_offset);
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
template <auto... Methods, typename T>
inline void InlineCallableBufferImpl<Allocator, Signatures...>::PushImplMethods(
    T&& callable) {
  using DecayedT = std::remove_cvref_t<T>;

  constexpr size_type element_size = sizeof(DecayedT);
  constexpr size_type element_align = alignof(DecayedT);
  constexpr size_type base_header = BaseHeaderSize();
  constexpr size_type fn_ptr_size = sizeof(FirstExecuteFn);

  // Align header to function pointer alignment
  auto header_offset = AlignUp(buffer_.size(), alignof(FirstExecuteFn));

  // Calculate data offset using relative offsets
  auto unaligned_data_offset = header_offset + base_header;
  auto aligned_data_offset = AlignUp(unaligned_data_offset, element_align);
  auto data_offset_from_header = aligned_data_offset - header_offset;
  auto total_entry_size = data_offset_from_header + element_size;

  auto required_size = header_offset + total_entry_size;

  // Grow the buffer properly if reallocation would be needed
  if (required_size > buffer_.capacity()) {
    GrowBuffer(required_size);
  }

  // Resize to final size (no reallocation since we ensured capacity above)
  buffer_.resize(required_size);

  auto* header_base = buffer_.data() + header_offset;

  // Store function pointers for each operation using custom methods
  StoreFunctionPointersMethods<DecayedT, Methods...>(
      header_offset, std::make_index_sequence<kNumOperations>{});

  // Store destroy function pointer
  auto* destroy_fn_ptr = reinterpret_cast<DestroyFn*>(
      header_base + (kNumOperations * fn_ptr_size));
  if constexpr (std::is_trivially_destructible_v<DecayedT>) {
    *destroy_fn_ptr = nullptr;
  } else {
    *destroy_fn_ptr = &DestroyCallable<DecayedT>;
  }

  // Store relocate function pointer
  auto* relocate_fn_ptr = reinterpret_cast<RelocateFn*>(
      header_base + (kNumOperations * fn_ptr_size) + sizeof(DestroyFn));
  if constexpr (std::is_trivially_copyable_v<DecayedT>) {
    *relocate_fn_ptr = nullptr;
  } else {
    *relocate_fn_ptr = &RelocateCallable<DecayedT>;
  }

  // Store data offset (relative to header start)
  auto* data_offset_storage = reinterpret_cast<size_type*>(
      header_base + (kNumOperations * fn_ptr_size) + sizeof(DestroyFn) +
      sizeof(RelocateFn));
  *data_offset_storage = data_offset_from_header;

  // Construct the callable in-place
  auto* data = header_base + data_offset_from_header;
  std::construct_at(reinterpret_cast<DecayedT*>(data),
                    std::forward<T>(callable));

  offsets_.push_back(header_offset);
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
template <typename T, size_t... Indices>
inline void InlineCallableBufferImpl<Allocator, Signatures...>::
    StoreFunctionPointersDefault(size_type header_offset,
                                 std::index_sequence<Indices...>) noexcept {
  auto* header_base = buffer_.data() + header_offset;

  (
      [&]<size_t Index>() {
        using ArgsTuple = details::NthSignatureArgsT<Index, Signatures...>;
        using ExecuteFn = details::TupleToFunctionPtrType<ArgsTuple>;

        [&]<typename... Args>(std::tuple<Args...>*) {
          auto* fn_ptr = reinterpret_cast<ExecuteFn*>(
              header_base + (Index * sizeof(FirstExecuteFn)));
          *fn_ptr = &ExecuteDefault<T, Index, Args...>;
        }(static_cast<ArgsTuple*>(nullptr));
      }.template operator()<Indices>(),
      ...);
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
template <typename T, auto... Methods, size_t... Indices>
inline void InlineCallableBufferImpl<Allocator, Signatures...>::
    StoreFunctionPointersMethods(size_type header_offset,
                                 std::index_sequence<Indices...>) noexcept {
  auto* header_base = buffer_.data() + header_offset;

  (
      [&]<size_t Index>() {
        using ArgsTuple = details::NthSignatureArgsT<Index, Signatures...>;
        using ExecuteFn = details::TupleToFunctionPtrType<ArgsTuple>;

        constexpr auto method = details::kNthMethod<Index, Methods...>;

        [&]<typename... Args>(std::tuple<Args...>*) {
          auto* fn_ptr = reinterpret_cast<ExecuteFn*>(
              header_base + (Index * sizeof(FirstExecuteFn)));
          *fn_ptr = &ExecuteMethod<T, method, Args...>;
        }(static_cast<ArgsTuple*>(nullptr));
      }.template operator()<Indices>(),
      ...);
}

template <typename Allocator, typename... Signatures>
  requires((sizeof...(Signatures) > 0) &&
           (details::VoidSignature<Signatures> && ...))
inline void* InlineCallableBufferImpl<Allocator, Signatures...>::GetDataPtr(
    size_type header_offset) const noexcept {
  auto* header_base = buffer_.data() + header_offset;
  const auto* data_offset_ptr = reinterpret_cast<const size_type*>(
      header_base + (kNumOperations * sizeof(FirstExecuteFn)) +
      sizeof(DestroyFn) + sizeof(RelocateFn));
  auto data_offset = *data_offset_ptr;
  return const_cast<std::byte*>(buffer_.data() + header_offset + data_offset);
}

}  // namespace roscraft::container
