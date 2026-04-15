#include <pch.hpp>

#include <roscraft/bridge/nodes/details/graph_utils.hpp>

#include <ament_index_cpp/get_resource.hpp>
#include <ament_index_cpp/get_resources.hpp>

#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace roscraft::bridge::details {

auto ReadInterfaceDefinition(const ParsedInterfaceType& interface_type)
    -> std::optional<std::string> {
  std::string unused_content;
  std::string package_prefix;
  if (!ament_index_cpp::get_resource("packages", interface_type.package,
                                     unused_content, &package_prefix))
      [[unlikely]] {
    return std::nullopt;
  }

  const auto definition_path =
      std::filesystem::path(package_prefix) / "share" / interface_type.package /
      interface_type.kind / (interface_type.name + "." + interface_type.kind);

  std::error_code error;
  if (!std::filesystem::is_regular_file(definition_path, error)) [[unlikely]] {
    return std::nullopt;
  }

  std::ifstream in(definition_path);
  if (!in.is_open()) [[unlikely]] {
    return std::nullopt;
  }

  std::string definition;
  try {
    std::stringstream ss;
    ss << in.rdbuf();
    definition = ss.str();
  } catch (...) {
    return std::nullopt;
  }

  if (!in.good() && !in.eof()) [[unlikely]] {
    return std::nullopt;
  }

  return definition;
}

std::string BuildFullyQualifiedNodeName(std::string_view node_name,
                                        std::string_view node_namespace) {
  size_t start = 0;
  while (start < node_name.length() && node_name[start] == '/') {
    ++start;
  }
  std::string_view canonical_name = node_name.substr(start);

  std::string canonical_namespace(node_namespace);
  if (canonical_namespace.empty()) {
    canonical_namespace = "/";
  } else {
    size_t end = node_namespace.length();
    while (end > 1 && node_namespace[end - 1] == '/') {
      end--;
    }
    canonical_namespace.assign(node_namespace, 0, end);
  }

  if (canonical_namespace.empty() || canonical_namespace == "/") {
    return std::format("/{}", canonical_name);
  }

  return std::format("{}/{}", canonical_namespace, canonical_name);
}

auto BuildTypeListFromResources(std::string_view kind,
                                std::string_view extension)
    -> std::vector<std::string> {
  std::vector<std::string> values;

  std::map<std::string, std::string> interface_packages;
  try {
    interface_packages = ament_index_cpp::get_resources("rosidl_interfaces");
  } catch (...) {
    return values;
  }

  values.reserve(interface_packages.size());

  const std::string prefix = std::format("{}/", kind);
  const std::string suffix = std::format(".{}", extension);

  for (const auto& [package_name, _prefix] : interface_packages) {
    std::string interface_content;
    if (!ament_index_cpp::get_resource("rosidl_interfaces", package_name,
                                       interface_content)) {
      continue;
    }

    std::string line;
    line.reserve(64);

    auto flush_line = [&line, &prefix, &suffix, &values, &package_name,
                       &interface_content, kind]() {
      if (line.empty()) {
        return;
      }

      if (!line.starts_with(prefix) || !line.ends_with(suffix) ||
          line.size() <= prefix.size() + suffix.size()) {
        line.clear();
        return;
      }

      const std::string_view interface_name(
          line.begin(), line.end() - suffix.size() - prefix.size());
      values.push_back(
          std::format("{}/{}/{}", package_name, kind, interface_name));
      line.clear();
    };

    for (char ch : interface_content) {
      if (ch == '\r') {
        continue;
      }
      if (ch == '\n') {
        flush_line();
        continue;
      }
      line.push_back(ch);
    }

    flush_line();
  }

  std::ranges::sort(values);
  auto [new_end, last] = std::ranges::unique(values);
  values.erase(new_end, values.end());
  return values;
}

}  // namespace roscraft::bridge::details
