#pragma once

#include <array>
#include <cstdint>

namespace roscraft::bridge::common {

enum class BackendType : uint8_t {
  kJni,
  kNetwork,
  kNone,
};

/**
 * @brief Returns enabled bridge backends.
 * @details The returned array always has two slots. Disabled slots contain
 *     BackendType::kNone.
 * @return Backends enabled at compile time.
 */
[[nodiscard]] auto AvailableBackends() noexcept
    -> const std::array<BackendType, 2>&;

/**
 * @brief Checks whether backend is enabled.
 * @param backend Backend to check.
 * @return True when the backend is enabled.
 */
[[nodiscard]] auto SupportsBackend(BackendType backend) noexcept -> bool;

}  // namespace roscraft::bridge::common
