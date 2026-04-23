#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace roscraft::bridge::details {

/**
 * @brief Structure holding parsed components of a ROS interface type.
 * @details Represents the package, kind (msg, srv, action), and name of an
 * interface.
 */
struct ParsedInterfaceType {
  std::string package;
  std::string kind;
  std::string name;
};

/**
 * @brief Checks if a ROS node name contains any segment starting with an
 * underscore '_'.
 * @param name The node name string view to check
 * @return True if the name is hidden, false otherwise
 */
[[nodiscard]] constexpr bool IsHiddenName(std::string_view name) noexcept {
  size_t segment_begin = 0;
  while (segment_begin <= name.size()) {
    const size_t segment_end = name.find('/', segment_begin);
    const size_t count = segment_end == std::string_view::npos
                             ? name.size() - segment_begin
                             : segment_end - segment_begin;

    const std::string_view segment = name.substr(segment_begin, count);
    if (!segment.empty() && segment.front() == '_') {
      return true;
    }

    if (segment_end == std::string_view::npos) {
      break;
    }
    segment_begin = segment_end + 1;
  }

  return false;
}

/**
 * @brief Parses a fully qualified interface type string (e.g.,
 * "package/kind/name").
 * @details The input must follow the strict format package/kind/name, where
 * kind is msg, srv, or action.
 * @param interface_type The full string view of the interface definition
 * @return An optional `ParsedInterfaceType` if parsing succeeds, otherwise
 * `std::nullopt`
 */
[[nodiscard]] constexpr auto ParseInterfaceType(std::string_view interface_type)
    -> std::optional<ParsedInterfaceType> {
  const size_t first_sep = interface_type.find('/');
  if (first_sep == std::string_view::npos || first_sep == 0 ||
      first_sep + 1 >= interface_type.size()) [[unlikely]] {
    return std::nullopt;
  }

  const size_t second_sep = interface_type.find('/', first_sep + 1);
  if (second_sep == std::string_view::npos || second_sep == first_sep + 1 ||
      second_sep + 1 >= interface_type.size()) [[unlikely]] {
    return std::nullopt;
  }

  if (interface_type.find('/', second_sep + 1) != std::string_view::npos)
      [[unlikely]] {
    return std::nullopt;
  }

  const std::string_view package = interface_type.substr(0, first_sep);
  const std::string_view kind =
      interface_type.substr(first_sep + 1, second_sep - (first_sep + 1));
  const std::string_view name = interface_type.substr(second_sep + 1);

  if (package.empty() || kind.empty() || name.empty()) [[unlikely]] {
    return std::nullopt;
  }

  // Validate Kind
  if (kind != "msg" && kind != "srv" && kind != "action") [[unlikely]] {
    return std::nullopt;
  }

  ParsedInterfaceType parsed{
      .package = std::string(package),
      .kind = std::string(kind),
      .name = std::string(name),
  };

  return parsed;
}

/**
 * @brief Canonicalizes a full node name (e.g., "ns/mynode" or
 * "/my/pkg/mynode").
 * @details Separates the given full name into its canonical node name and
 * namespace. The resulting namespace will always be correctly structured (e.g.,
 * ending in `/`).
 * @param full_name The fully qualified node name string view
 * @return An optional `std::pair<std::string, std::string>` containing
 * {node_name, node_namespace}, or `std::nullopt` if the input is invalid or
 * empty
 */
[[nodiscard]] constexpr auto CanonicalizeNodeNameAndNamespace(
    std::string_view full_name)
    -> std::optional<std::pair<std::string, std::string>> {
  if (full_name.empty()) [[unlikely]] {
    return std::nullopt;
  }

  std::string name(full_name);
  if (!name.starts_with('/')) {
    name.insert(name.begin(), '/');
  }

  const size_t last_sep = name.find_last_of('/');
  if (last_sep == std::string::npos) [[unlikely]] {
    return std::nullopt;
  }

  std::string node_name;
  std::string node_namespace;

  // If the separator is at index 0, the namespace should be root
  if (last_sep == 0) {
    node_name.assign(name, 1);
    node_namespace = "/";
  } else {
    node_name.assign(name, last_sep + 1);
    node_namespace.assign(name, 0, last_sep + 1);
  }

  if (node_name.empty() || node_namespace.empty()) [[unlikely]] {
    return std::nullopt;
  }

  // Clean up trailing slashes from namespace component for consistency, unless
  // it's just "/"
  if (node_namespace.length() > 1 && node_namespace.back() == '/') {
    node_namespace.pop_back();
  }

  return std::make_pair(std::move(node_name), std::move(node_namespace));
}

/**
 * @brief Reads the source code definition file for a given interface type.
 * @param interface_type The fully qualified parsed interface type.
 * @return An optional string containing the content of the definition file, or
 * `std::nullopt` if reading fails
 */
[[nodiscard]] auto ReadInterfaceDefinition(
    const ParsedInterfaceType& interface_type) -> std::optional<std::string>;

/**
 * @brief Builds a fully qualified and canonicalized ROS node name from separate
 * components.
 * @details Ensures correct separation using slashes and handles redundant
 * leading/trailing slashes.
 * @param node_name The primary component of the node name
 * @param node_namespace The namespace component
 * @return The fully qualified name string, e.g., "ns/mynode"
 */
[[nodiscard]] std::string BuildFullyQualifiedNodeName(
    std::string_view node_name, std::string_view node_namespace);

/**
 * @brief Generates a list of all available interfaces (msg, srv, action) for a
 * given kind.
 * @details This function reads package metadata using `ament_index` and
 * collects unique interface names.
 * @param kind The type kind (e.g., "msg", "srv")
 * @param extension The file extension (e.g., ".proto")
 * @return A sorted, unique vector of fully qualified resource names
 * (package/kind/name)
 */
[[nodiscard]] auto BuildTypeListFromResources(std::string_view kind,
                                              std::string_view extension)
    -> std::vector<std::string>;

}  // namespace roscraft::bridge::details
