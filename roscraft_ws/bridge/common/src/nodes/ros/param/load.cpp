#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/param/load.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/error.hpp>
#include <roscraft/bridge/command/types/param.hpp>
#include <roscraft/bridge/nodes/ros/common.hpp>

#include <glaze/yaml.hpp>

#include <rclcpp/executor.hpp>
#include <rclcpp/parameter_client.hpp>
#include <rclcpp/rclcpp.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <expected>
#include <format>
#include <limits>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

namespace {

using GenericYamlValue = glz::generic;
using GenericYamlObject = GenericYamlValue::object_t;
using GenericYamlArray = GenericYamlValue::array_t;

constexpr std::string_view kRosParametersKey = "ros__parameters";
constexpr std::string_view kWildcardNodeName = "/**";

[[nodiscard]] auto MakeNodeLookupCandidates(std::string_view node_name)
    -> std::vector<std::string> {
  std::vector<std::string> candidates;

  const auto append_unique = [&candidates](std::string_view candidate) {
    if (candidate.empty()) {
      return;
    }
    if (std::ranges::find(candidates, candidate) == candidates.end()) {
      candidates.emplace_back(candidate);
    }
  };

  append_unique(node_name);

  if (node_name.starts_with('/')) {
    append_unique(node_name.substr(1));
  } else {
    std::string with_slash(1, '/');
    with_slash.append(node_name);
    append_unique(with_slash);
  }

  return candidates;
}

[[nodiscard]] bool ContainsNodeScopedSections(const GenericYamlObject& root,
                                              std::string_view exclude_key) {
  return std::ranges::any_of(root, [&exclude_key](const auto& entry) {
    if (!entry.first.empty() && entry.first == exclude_key) {
      return false;
    }

    const auto& value = entry.second;
    if (!value.is_object()) {
      return false;
    }

    const auto& object = value.get_object();
    return object.find(kRosParametersKey) != object.end();
  });
}

[[nodiscard]] auto TrySelectNodeSection(const GenericYamlObject& root,
                                        std::string_view key,
                                        const GenericYamlObject*& out)
    -> std::expected<bool, std::string> {
  const auto node_iter = root.find(key);
  if (node_iter == root.end()) {
    return false;
  }

  if (!node_iter->second.is_object()) [[unlikely]] {
    return std::unexpected(
        std::format("YAML node section '{}' must be a mapping", key));
  }

  const auto& node_section = node_iter->second.get_object();
  const auto ros_parameters_iter = node_section.find(kRosParametersKey);
  if (ros_parameters_iter == node_section.end()) {
    out = &node_section;
    return true;
  }

  if (!ros_parameters_iter->second.is_object()) [[unlikely]] {
    return std::unexpected(
        std::format("YAML key '{}' in section '{}' must be a mapping",
                    kRosParametersKey, key));
  }

  out = &ros_parameters_iter->second.get_object();
  return true;
}

[[nodiscard]] auto SelectParameterRoots(
    const GenericYamlObject& root, std::string_view requested_node_name,
    bool use_wildcard,
    std::vector<std::reference_wrapper<const GenericYamlObject>>& out)
    -> std::expected<bool, std::string> {
  const GenericYamlObject* exact_node_section = nullptr;
  for (const auto& candidate : MakeNodeLookupCandidates(requested_node_name)) {
    const GenericYamlObject* resolved = nullptr;
    auto result = TrySelectNodeSection(root, candidate, resolved);
    if (!result) {
      return std::unexpected(result.error());
    }
    if (*result) {
      exact_node_section = resolved;
      break;
    }
  }

  const GenericYamlObject* wildcard_section = nullptr;
  if (use_wildcard) {
    auto result =
        TrySelectNodeSection(root, kWildcardNodeName, wildcard_section);
    if (!result) {
      return std::unexpected(result.error());
    }
  }

  if (wildcard_section != nullptr) {
    out.push_back(std::ref(*wildcard_section));
  }
  if (exact_node_section != nullptr) {
    out.push_back(std::ref(*exact_node_section));
  }
  if (!out.empty()) {
    return true;
  }

  const auto ros_parameters_iter = root.find(kRosParametersKey);
  if (ros_parameters_iter != root.end()) {
    if (!ros_parameters_iter->second.is_object()) [[unlikely]] {
      return std::unexpected(
          std::format("YAML key '{}' must be a mapping", kRosParametersKey));
    }

    out.push_back(std::ref(ros_parameters_iter->second.get_object()));
    return true;
  }

  if (ContainsNodeScopedSections(root, "")) {
    if (use_wildcard) [[unlikely]] {
      return std::unexpected(std::format("No YAML section found for node '{}'",
                                         requested_node_name));
    }

    if (ContainsNodeScopedSections(root, kWildcardNodeName)) [[unlikely]] {
      return std::unexpected(std::format("No YAML section found for node '{}'",
                                         requested_node_name));
    }

    return true;
  }

  out.push_back(std::ref(root));
  return true;
}

[[nodiscard]] constexpr bool IsIntegerRepresentable(double value) noexcept {
  constexpr auto min_as_double =
      static_cast<double>(std::numeric_limits<int64_t>::min());
  constexpr auto max_as_double =
      static_cast<double>(std::numeric_limits<int64_t>::max());
  return value >= min_as_double && value <= max_as_double;
}

[[nodiscard]] bool IsIntegralNumber(double value) noexcept {
  return std::isfinite(value) && std::floor(value) == value;
}

[[nodiscard]] auto ParseScalarValue(const GenericYamlValue& yaml_value,
                                    std::string_view parameter_name,
                                    rclcpp::ParameterValue& out)
    -> std::expected<bool, std::string> {
  if (yaml_value.is_boolean()) {
    out = rclcpp::ParameterValue(yaml_value.get_boolean());
    return true;
  }

  if (yaml_value.is_string()) {
    out = rclcpp::ParameterValue(yaml_value.get_string());
    return true;
  }

  if (yaml_value.is_number()) {
    const double number = yaml_value.get_number();
    if (!std::isfinite(number)) [[unlikely]] {
      return std::unexpected(std::format(
          "Parameter '{}' has non-finite numeric value", parameter_name));
    }

    if (IsIntegralNumber(number) && IsIntegerRepresentable(number)) {
      out = rclcpp::ParameterValue(static_cast<int64_t>(number));
      return true;
    }

    out = rclcpp::ParameterValue(number);
    return true;
  }

  if (yaml_value.is_null()) [[unlikely]] {
    return std::unexpected(
        std::format("Parameter '{}' uses null value, which is unsupported",
                    parameter_name));
  }

  return std::unexpected(std::format(
      "Parameter '{}' has unsupported complex scalar value", parameter_name));
}

[[nodiscard]] auto ParseArrayValue(const GenericYamlArray& yaml_array,
                                   std::string_view parameter_name,
                                   rclcpp::ParameterValue& out)
    -> std::expected<bool, std::string> {
  if (yaml_array.empty()) [[unlikely]] {
    return std::unexpected(
        std::format("Parameter '{}' uses empty array, which cannot be typed",
                    parameter_name));
  }

  const auto& first = yaml_array.front();

  if (first.is_boolean()) {
    std::vector<bool> values;
    values.reserve(yaml_array.size());
    for (const auto& element : yaml_array) {
      if (!element.is_boolean()) [[unlikely]] {
        return std::unexpected(std::format(
            "Parameter '{}' array has mixed element types", parameter_name));
      }
      values.push_back(element.get_boolean());
    }

    out = rclcpp::ParameterValue(std::move(values));
    return true;
  }

  if (first.is_string()) {
    std::vector<std::string> values;
    values.reserve(yaml_array.size());
    for (const auto& element : yaml_array) {
      if (!element.is_string()) [[unlikely]] {
        return std::unexpected(std::format(
            "Parameter '{}' array has mixed element types", parameter_name));
      }
      values.push_back(element.get_string());
    }

    out = rclcpp::ParameterValue(std::move(values));
    return true;
  }

  if (first.is_number()) {
    bool requires_double = false;
    for (const auto& element : yaml_array) {
      if (!element.is_number()) [[unlikely]] {
        return std::unexpected(std::format(
            "Parameter '{}' array has mixed element types", parameter_name));
      }

      const double number = element.get_number();
      if (!std::isfinite(number)) [[unlikely]] {
        return std::unexpected(
            std::format("Parameter '{}' array has non-finite numeric value",
                        parameter_name));
      }

      if (!IsIntegralNumber(number) || !IsIntegerRepresentable(number)) {
        requires_double = true;
      }
    }

    if (requires_double) {
      std::vector<double> values;
      values.reserve(yaml_array.size());
      for (const auto& element : yaml_array) {
        values.push_back(element.get_number());
      }

      out = rclcpp::ParameterValue(std::move(values));
      return true;
    }

    std::vector<int64_t> values;
    values.reserve(yaml_array.size());
    for (const auto& element : yaml_array) {
      values.push_back(static_cast<int64_t>(element.get_number()));
    }

    out = rclcpp::ParameterValue(std::move(values));
    return true;
  }

  return std::unexpected(std::format(
      "Parameter '{}' array element type is unsupported", parameter_name));
}

[[nodiscard]] std::string JoinParameterName(std::string_view prefix,
                                            std::string_view key) {
  if (prefix.empty()) {
    return std::string(key);
  }

  std::string joined(prefix);
  joined.push_back('.');
  joined.append(key);
  return joined;
}

void UpsertParameter(std::vector<rclcpp::Parameter>& out,
                     rclcpp::Parameter parameter) {
  const auto existing =
      std::ranges::find_if(out, [&parameter](const rclcpp::Parameter& value) {
        return value.get_name() == parameter.get_name();
      });
  if (existing != out.end()) {
    *existing = std::move(parameter);
    return;
  }

  out.push_back(std::move(parameter));
}

[[nodiscard]] auto FlattenParameterMap(const GenericYamlObject& yaml_map,
                                       std::string_view prefix,
                                       std::vector<rclcpp::Parameter>& out)
    -> std::expected<bool, std::string> {
  for (const auto& entry : yaml_map) {
    const auto& key = entry.first;
    const auto& value = entry.second;

    if (key.empty()) [[unlikely]] {
      return std::unexpected("Encountered empty parameter key in YAML mapping");
    }

    const std::string parameter_name = JoinParameterName(prefix, key);
    if (value.is_object()) {
      auto result =
          FlattenParameterMap(value.get_object(), parameter_name, out);
      if (!result) {
        return std::unexpected(result.error());
      }
      continue;
    }

    rclcpp::ParameterValue typed_value;
    if (value.is_array()) {
      auto result =
          ParseArrayValue(value.get_array(), parameter_name, typed_value);
      if (!result) {
        return std::unexpected(result.error());
      }
    } else {
      auto result = ParseScalarValue(value, parameter_name, typed_value);
      if (!result) {
        return std::unexpected(result.error());
      }
    }

    out.emplace_back(parameter_name, std::move(typed_value));
  }

  return true;
}

[[nodiscard]] auto ParseYamlParameters(std::string_view yaml_text,
                                       std::string_view requested_node_name,
                                       bool use_wildcard,
                                       std::vector<rclcpp::Parameter>& out)
    -> std::expected<bool, std::string> {
  GenericYamlValue yaml_root;
  const auto read_error = glz::read_yaml(yaml_root, yaml_text);
  if (read_error) [[unlikely]] {
    return std::unexpected(glz::format_error(read_error, yaml_text));
  }

  if (!yaml_root.is_object()) [[unlikely]] {
    return std::unexpected("YAML root must be a mapping");
  }

  std::vector<std::reference_wrapper<const GenericYamlObject>> parameter_roots;
  auto roots_result =
      SelectParameterRoots(yaml_root.get_object(), requested_node_name,
                           use_wildcard, parameter_roots);
  if (!roots_result) {
    return std::unexpected(roots_result.error());
  }

  for (const auto parameter_root : parameter_roots) {
    std::vector<rclcpp::Parameter> parsed;
    auto result = FlattenParameterMap(parameter_root.get(), "", parsed);
    if (!result) {
      return std::unexpected(result.error());
    }

    for (auto& parameter : parsed) {
      UpsertParameter(out, std::move(parameter));
    }
  }

  return true;
}

}  // namespace

ParamLoadNode::ParamLoadNode(CommandQueue& incoming, CommandQueue& outgoing,
                             std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_param_load_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      param_load_consumer_(incoming.MakeConsumerToken<ParamLoadCmd>()),
      param_load_response_producer_(
          outgoing.MakeProducerToken<ParamLoadResponseCmd>()),
      error_producer_(outgoing.MakeProducerToken<ErrorCmd>()),
      temp_node_([] {
        auto options = rclcpp::NodeOptions()
                           .start_parameter_services(false)
                           .start_parameter_event_publisher(false)
                           .enable_rosout(false)
                           .use_global_arguments(false);
        return std::make_shared<rclcpp::Node>("_roscraft_param_load_internal",
                                              options);
      }()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void ParamLoadNode::DrainParamLoadCommands() {
  auto& in_storage = incoming_.get().TypedStorage<ParamLoadCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<ParamLoadResponseCmd>();

  ParamLoadCmd cmd(std::pmr::get_default_resource());
  while (in_storage.Dequeue(param_load_consumer_, cmd)) {
    if (cmd.node_name.empty()) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_LOAD_FAILED",
                "node_name must be non-empty");
      continue;
    }

    if (cmd.yaml_text.empty()) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_LOAD_FAILED",
                "yaml_text must be non-empty");
      continue;
    }

    const auto timeout = ResolveTimeout(cmd.timeout_seconds);
    const auto timeout_for_client = std::chrono::duration<double>(timeout);

    std::shared_ptr<rclcpp::SyncParametersClient> client;
    auto client_it = parameter_clients_.find(cmd.node_name);
    if (client_it != parameter_clients_.end()) {
      client = client_it->second;
    } else {
      std::string node_name(cmd.node_name);
      client =
          std::make_shared<rclcpp::SyncParametersClient>(temp_node_, node_name);
      const auto [inserted_it, inserted] =
          parameter_clients_.emplace(std::move(node_name), client);
      client_it = inserted_it;
    }

    if (!client->wait_for_service(timeout)) [[unlikely]] {
      parameter_clients_.erase(client_it);
      SendError(cmd.request_id, "PARAM_LOAD_FAILED",
                "Parameter services unavailable before timeout");
      continue;
    }

    std::vector<rclcpp::Parameter> parameters;
    auto parse_result = ParseYamlParameters(cmd.yaml_text, cmd.node_name,
                                            cmd.use_wildcard, parameters);
    if (!parse_result) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_LOAD_FAILED", parse_result.error());
      continue;
    }

    ParamLoadResponseCmd response(std::pmr::get_default_resource());
    response.request_id = cmd.request_id;
    response.node_name = cmd.node_name;
    response.params_loaded = static_cast<uint32_t>(parameters.size());

    if (parameters.empty()) {
      response.success = true;
      response.reason = "No parameters found in input";
      out_storage.Enqueue(param_load_response_producer_, std::move(response));
      continue;
    }

    std::vector<rcl_interfaces::msg::SetParametersResult> results;
    try {
      results = client->set_parameters(parameters, timeout_for_client);
    } catch (const std::exception& ex) {
      SendError(cmd.request_id, "PARAM_LOAD_FAILED", ex.what());
      continue;
    }

    response.success = std::ranges::all_of(
        results, [](const auto& result) { return result.successful; });

    if (response.success) {
      response.reason = "Parameters loaded";
    } else {
      response.reason = "One or more parameter updates failed";
      for (const auto& result : results) {
        if (!result.successful && !result.reason.empty()) {
          response.reason = result.reason;
          break;
        }
      }
    }

    out_storage.Enqueue(param_load_response_producer_, std::move(response));
  }
}

void ParamLoadNode::OnPollTimer() {
  DrainParamLoadCommands();
}

void ParamLoadNode::SendError(uint64_t request_id, std::string_view error_code,
                              std::string_view error_message) {
  ErrorCmd cmd(std::pmr::get_default_resource());
  cmd.request_id = request_id;
  cmd.error_code = std::pmr::string(error_code);
  cmd.error_message = std::pmr::string(error_message);

  outgoing_.get().Enqueue(error_producer_, std::move(cmd));
}

}  // namespace roscraft::bridge
