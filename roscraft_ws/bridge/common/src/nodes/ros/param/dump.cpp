#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/param/dump.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/error.hpp>
#include <roscraft/bridge/command/types/param.hpp>
#include <roscraft/bridge/nodes/ros/common.hpp>

#include <rclcpp/parameter_client.hpp>
#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

ParamDumpNode::ParamDumpNode(CommandQueue& incoming, CommandQueue& outgoing,
                             std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_param_dump_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      param_dump_consumer_(incoming.MakeConsumerToken<ParamDumpCmd>()),
      param_dump_response_producer_(
          outgoing.MakeProducerToken<ParamDumpResponseCmd>()),
      error_producer_(outgoing.MakeProducerToken<ErrorCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void ParamDumpNode::DrainParamDumpCommands() {
  auto& in_storage = incoming_.get().TypedStorage<ParamDumpCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<ParamDumpResponseCmd>();

  ParamDumpCmd cmd(allocator_);
  while (in_storage.Dequeue(param_dump_consumer_, cmd)) {
    if (cmd.node_name.empty()) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_DUMP_FAILED",
                "node_name must be non-empty");
      continue;
    }

    const auto timeout = ResolveTimeout(cmd.timeout_seconds);
    const auto timeout_for_client = std::chrono::duration<double>(timeout);
    auto client = std::make_shared<rclcpp::SyncParametersClient>(
        this, std::string(cmd.node_name));
    if (!client->wait_for_service(timeout)) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_DUMP_FAILED",
                "Parameter services unavailable before timeout");
      continue;
    }

    std::vector<std::string> prefixes;
    prefixes.reserve(cmd.prefixes.size());
    for (const auto& prefix : cmd.prefixes) {
      prefixes.emplace_back(prefix);
    }

    rcl_interfaces::msg::ListParametersResult listed;
    try {
      listed = client->list_parameters(prefixes, 0, timeout_for_client);
    } catch (const std::exception& ex) {
      SendError(cmd.request_id, "PARAM_DUMP_FAILED", ex.what());
      continue;
    }

    std::vector<rclcpp::Parameter> parameters;
    if (!listed.names.empty()) {
      try {
        parameters = client->get_parameters(listed.names, timeout_for_client);
      } catch (const std::exception& ex) {
        SendError(cmd.request_id, "PARAM_DUMP_FAILED", ex.what());
        continue;
      }
    }

    ParamDumpResponseCmd response(allocator_);
    response.request_id = cmd.request_id;
    response.node_name = cmd.node_name;

    std::string yaml;
    yaml.reserve(parameters.size() * 32);
    for (size_t idx = 0; idx < parameters.size(); ++idx) {
      const auto& parameter = parameters[idx];
      if (parameter.get_type() == rclcpp::ParameterType::PARAMETER_NOT_SET) {
        continue;
      }

      yaml.append(parameter.get_name());
      yaml.append(": ");
      yaml.append(parameter.value_to_string());
      yaml.push_back('\n');
    }
    response.yaml_text = std::move(yaml);

    out_storage.Enqueue(param_dump_response_producer_, std::move(response));
  }
}

void ParamDumpNode::OnPollTimer() {
  DrainParamDumpCommands();
}

void ParamDumpNode::SendError(uint64_t request_id, std::string_view error_code,
                              std::string_view error_message) {
  ErrorCmd cmd(allocator_);
  cmd.request_id = request_id;
  cmd.error_code = std::pmr::string(error_code, allocator_);
  cmd.error_message = std::pmr::string(error_message, allocator_);

  outgoing_.get().Enqueue(error_producer_, std::move(cmd));
}

}  // namespace roscraft::bridge
