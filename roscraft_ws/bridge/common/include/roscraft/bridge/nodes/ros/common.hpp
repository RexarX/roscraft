#pragma once

#include <roscraft/utils/common_traits.hpp>

#include <chrono>
#include <cmath>
#include <concepts>
#include <limits>

namespace roscraft::bridge {

inline constexpr std::chrono::seconds kDefaultTimeout{5};

template <utils::ArithmeticTrait T,
          typename DurationT = std::chrono::nanoseconds>
[[nodiscard]] constexpr DurationT ResolveTimeout(
    T timeout_seconds,
    DurationT default_timeout =
        std::chrono::duration_cast<DurationT>(kDefaultTimeout)) {
  if constexpr (std::floating_point<T>) {
    if (timeout_seconds < std::numeric_limits<T>::epsilon()) {
      return std::chrono::duration_cast<DurationT>(default_timeout);
    }
  } else if constexpr (std::integral<T>) {
    if (timeout_seconds <= T{0}) {
      return std::chrono::duration_cast<DurationT>(default_timeout);
    }
  }

  return std::chrono::duration_cast<DurationT>(
      std::chrono::duration<double>(timeout_seconds));
}

}  // namespace roscraft::bridge
