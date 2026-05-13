#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/param/set.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/error.hpp>
#include <roscraft/bridge/command/types/param.hpp>
#include <roscraft/bridge/nodes/ros/common.hpp>

#include <rclcpp/executor.hpp>
#include <rclcpp/parameter_client.hpp>
#include <rclcpp/rclcpp.hpp>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

namespace {

[[nodiscard]] auto TryParseBoolValue(std::string_view value)
    -> std::optional<bool> {
  std::string text(value);
  std::erase_if(text, [](unsigned char ch) { return std::isspace(ch) != 0; });
  std::ranges::transform(text, text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });

  if (text == "true" || text == "1") {
    return true;
  }
  if (text == "false" || text == "0") {
    return false;
  }
  return std::nullopt;
}

[[nodiscard]] auto TryParseIntValue(std::string_view value)
    -> std::optional<int64_t> {
  std::string text(value);
  std::erase_if(text, [](unsigned char ch) { return std::isspace(ch) != 0; });
  if (text.empty()) {
    return std::nullopt;
  }

  int64_t parsed = 0;
  const auto* begin = text.data();
  const auto* end = text.data() + text.size();
  const auto result = std::from_chars(begin, end, parsed, 10);
  if (result.ec != std::errc{} || result.ptr != end) [[unlikely]] {
    return std::nullopt;
  }
  return parsed;
}

[[nodiscard]] auto TryParseDoubleValue(std::string_view value)
    -> std::optional<double> {
  std::string text(value);
  std::erase_if(text, [](unsigned char ch) { return std::isspace(ch) != 0; });
  if (text.empty()) [[unlikely]] {
    return std::nullopt;
  }

  char* parse_end = nullptr;
  errno = 0;
  const double parsed = std::strtod(text.c_str(), &parse_end);
  if (parse_end == nullptr || parse_end != text.data() + text.size())
      [[unlikely]] {
    return std::nullopt;
  }
  if (errno == ERANGE || !std::isfinite(parsed)) [[unlikely]] {
    return std::nullopt;
  }
  return parsed;
}

[[nodiscard]] rclcpp::Parameter BuildTypedParameter(
    const std::string& name, std::string_view value_text) {
  if (const auto parsed_bool = TryParseBoolValue(value_text);
      parsed_bool.has_value()) {
    return rclcpp::Parameter(name, *parsed_bool);
  }

  if (const auto parsed_int = TryParseIntValue(value_text);
      parsed_int.has_value()) {
    return rclcpp::Parameter(name, *parsed_int);
  }

  if (const auto parsed_double = TryParseDoubleValue(value_text);
      parsed_double.has_value()) {
    return rclcpp::Parameter(name, *parsed_double);
  }

  return rclcpp::Parameter(name, std::string(value_text));
}

}  // namespace

ParamSetNode::ParamSetNode(CommandQueue& incoming, CommandQueue& outgoing,
                           std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_param_set_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      param_set_consumer_(incoming.MakeConsumerToken<ParamSetCmd>()),
      param_set_response_producer_(
          outgoing.MakeProducerToken<ParamSetResponseCmd>()),
      error_producer_(outgoing.MakeProducerToken<ErrorCmd>()),
      temp_node_([] {
        auto options = rclcpp::NodeOptions()
                           .start_parameter_services(false)
                           .start_parameter_event_publisher(false)
                           .enable_rosout(false)
                           .use_global_arguments(false);
        return std::make_shared<rclcpp::Node>("_roscraft_param_set_internal",
                                              options);
      }()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void ParamSetNode::DrainParamSetCommands() {
  auto& in_storage = incoming_.get().TypedStorage<ParamSetCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<ParamSetResponseCmd>();

  ParamSetCmd cmd(std::pmr::get_default_resource());
  while (in_storage.Dequeue(param_set_consumer_, cmd)) {
    if (cmd.node_name.empty() || cmd.param_name.empty()) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_SET_FAILED",
                "node_name and param_name must be non-empty");
      continue;
    }

    const auto timeout = ResolveTimeout(cmd.timeout_seconds);
    const auto timeout_for_client = std::chrono::duration<double>(timeout);
    auto client = std::make_shared<rclcpp::SyncParametersClient>(
        temp_node_, std::string(cmd.node_name));
    if (!client->wait_for_service(timeout)) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_SET_FAILED",
                "Parameter services unavailable before timeout");
      continue;
    }

    const auto typed_parameter =
        BuildTypedParameter(std::string(cmd.param_name), cmd.value_text);

    ParamSetResponseCmd response(std::pmr::get_default_resource());
    response.request_id = cmd.request_id;
    response.node_name = std::move(cmd.node_name);
    response.param_name = std::move(cmd.param_name);
    response.param_type = rclcpp::to_string(typed_parameter.get_type());
    response.value_text = typed_parameter.value_to_string();

    std::vector<rcl_interfaces::msg::SetParametersResult> results;
    try {
      results = client->set_parameters({typed_parameter}, timeout_for_client);
    } catch (const std::exception& ex) {
      SendError(cmd.request_id, "PARAM_SET_FAILED", ex.what());
      continue;
    }

    if (!results.empty()) {
      response.success = results.front().successful;
      response.reason = results.front().reason;
    } else {
      response.success = false;
      response.reason = "No result from parameter service";
    }

    out_storage.Enqueue(param_set_response_producer_, std::move(response));
  }
}

void ParamSetNode::OnPollTimer() {
  DrainParamSetCommands();
}

void ParamSetNode::SendError(uint64_t request_id, std::string_view error_code,
                             std::string_view error_message) {
  ErrorCmd cmd(std::pmr::get_default_resource());
  cmd.request_id = request_id;
  cmd.error_code = std::pmr::string(error_code);
  cmd.error_message = std::pmr::string(error_message);

  outgoing_.get().Enqueue(error_producer_, std::move(cmd));
}

}  // namespace roscraft::bridge
