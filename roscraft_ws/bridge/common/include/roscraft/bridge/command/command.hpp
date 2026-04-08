#pragma once

#include <roscraft/utils/type_info.hpp>

#include <concepts>
#include <cstdint>
#include <expected>
#include <string_view>
#include <type_traits>

namespace roscraft::bridge {

/// @brief Type index for commands.
using CommandTypeIndex = utils::TypeIndex;

/**
 * @brief Concept for valid command types.
 * @details A command must be destructible, default initializable,
 * move constructible and be an object.
 */
template <typename T>
concept CommandTrait =
    std::destructible<T> && std::default_initializable<T> &&
    std::move_constructible<T> && std::is_object_v<std::remove_cvref_t<T>>;

/**
 * @brief Concept for commands that provide custom names.
 * @details A command with name trait must satisfy `CommandTrait and provide:
 * - `static constexpr std::string_view kName` variable
 */
template <typename T>
concept CommandWithNameTrait = CommandTrait<T> && requires {
  { std::remove_cvref_t<T>::kName } -> std::convertible_to<std::string_view>;
};

/**
 * @brief Retrieves the unqualified type name of a command type.
 * @tparam T Command type satisfying `CommandTrait`
 * @return Unqualified type name as string_view
 */
template <CommandTrait T>
[[nodiscard]] consteval std::string_view CommandNameOf() noexcept {
  if constexpr (CommandWithNameTrait<T>) {
    return T::kName;
  } else {
    return utils::TypeNameOf<T>();
  }
}

/**
 * @brief Retrieves the unqualified type name of a command type.
 * @tparam T Command type satisfying `CommandTrait`
 * @param instance Command instance
 * @return Unqualified type name as string_view
 */
template <CommandTrait T>
[[nodiscard]] consteval std::string_view CommandNameOf(
    const T& /*instance*/) noexcept {
  return CommandNameOf<std::remove_cvref_t<T>>();
}

}  // namespace roscraft::bridge
