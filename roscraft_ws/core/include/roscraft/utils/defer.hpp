#pragma once

#include <roscraft/compiler.hpp>
#include <roscraft/utils/macro.hpp>

#include <concepts>
#include <type_traits>
#include <utility>

namespace roscraft::utils {

/**
 * @brief A utility class that defers the execution of a callable until the
 * object goes out of scope.
 * @tparam F The type of the callable to be executed upon destruction.
 *
 * @example
 * @code
 * // Pattern 1: Pre-defined lambda
 * auto cleanup = [resource]() { delete resource; };
 * ROSCRAFT_DEFER_CALL(cleanup);
 *
 * // Pattern 2: Inline lambda (no capture list needed)
 * ROSCRAFT_DEFER {
 *   delete resource;
 * };
 * @endcode
 */
template <std::invocable F>
class Defer {
public:
  Defer() = delete;

  /**
   * @brief Constructs a Defer object that will execute the provided callable
   * upon destruction.
   * @param func The callable to be executed.
   */
  constexpr explicit Defer(F func) noexcept(
      std::is_nothrow_move_constructible_v<F>)
      : func_(std::move(func)) {}
  Defer(Defer&&) = delete;
  Defer(const Defer&) = delete;
  constexpr ~Defer() noexcept(std::is_nothrow_invocable_v<F>) { func_(); }

  Defer& operator=(const Defer&) = delete;
  Defer& operator=(Defer&&) = delete;

private:
  F func_;
};

namespace details {

/// @brief Helper struct for the `ROSCRAFT_DEFER` macro to enable inline lambda
/// syntax.
struct DeferHelper {
  template <std::invocable F>
  constexpr auto operator+(F&& func) noexcept(
      std::is_nothrow_constructible_v<Defer<std::remove_cvref_t<F>>, F>)
      -> Defer<std::remove_cvref_t<F>> {
    return Defer<std::remove_cvref_t<F>>(std::forward<F>(func));
  }
};

}  // namespace details

}  // namespace roscraft::utils

/**
 * @brief Defers execution of an inline lambda until the end of the current
 * scope.
 * @details The lambda is written directly after the macro without explicit
 * capture list.
 *
 * @example
 * @code
 * int* ptr = new int(42);
 * ROSCRAFT_DEFER {
 *   delete ptr;
 * };
 * @endcode
 */
#define ROSCRAFT_DEFER                               \
  const auto ROSCRAFT_CONCAT(_defer_, __COUNTER__) = \
      ::roscraft::utils::details::DeferHelper() + [&] ROSCRAFT_ALWAYS_INLINE()

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/**
 * @brief Defers execution of a callable until the end of the current scope.
 * @details Accepts lambdas, functors, function pointers, and `std::function`
 * objects.
 *
 * @example
 * @code
 * auto cleanup = []() { std::cout << "Cleanup\n"; };
 * ROSCRAFT_DEFER_CALL(cleanup);
 * @endcode
 */
#define ROSCRAFT_DEFER_CALL(callable)                \
  const auto ROSCRAFT_CONCAT(_defer_, __COUNTER__) = \
      ::roscraft::utils::Defer(callable)

// NOLINTEND(cppcoreguidelines-macro-usage)
