#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/param/describe.hpp>

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
#include <format>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

namespace {

[[nodiscard]] std::pmr::string BuildDescriptorConstraintsText(
    const rcl_interfaces::msg::ParameterDescriptor& descriptor,
    std::pmr::memory_resource* allocator = std::pmr::get_default_resource()) {
  std::pmr::string result{allocator};

  if (!descriptor.additional_constraints.empty()) {
    result.append(descriptor.additional_constraints);
  }

  if (!descriptor.integer_range.empty()) {
    const auto& range = descriptor.integer_range.front();

    const size_t formatted_size =
        std::formatted_size("; integer_range=[{}, {}], step={}",
                            range.from_value, range.to_value, range.step);
    result.reserve(result.capacity() + formatted_size);
    std::format_to(std::back_inserter(result),
                   "; integer_range=[{}, {}], step={}", range.from_value,
                   range.to_value, range.step);
  }

  if (!descriptor.floating_point_range.empty()) {
    const auto& range = descriptor.floating_point_range.front();

    const size_t formatted_size =
        std::formatted_size("; floating_range=[{}, {}], step={}",
                            range.from_value, range.to_value, range.step);
    result.reserve(result.capacity() + formatted_size);
    std::format_to(std::back_inserter(result),
                   "; floating_range=[{}, {}], step={}", range.from_value,
                   range.to_value, range.step);
  }

  return result;
}

}  // namespace

ParamDescribeNode::ParamDescribeNode(CommandQueue& incoming,
                                     CommandQueue& outgoing,
                                     std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_param_describe_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      param_describe_consumer_(incoming.MakeConsumerToken<ParamDescribeCmd>()),
      param_describe_response_producer_(
          outgoing.MakeProducerToken<ParamDescribeResponseCmd>()),
      error_producer_(outgoing.MakeProducerToken<ErrorCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void ParamDescribeNode::DrainParamDescribeCommands() {
  auto& in_storage = incoming_.get().TypedStorage<ParamDescribeCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<ParamDescribeResponseCmd>();

  ParamDescribeCmd cmd(allocator_);
  while (in_storage.Dequeue(param_describe_consumer_, cmd)) {
    if (cmd.node_name.empty() || cmd.param_name.empty()) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_DESCRIBE_FAILED",
                "node_name and param_name must be non-empty");
      continue;
    }

    const auto timeout = ResolveTimeout(cmd.timeout_seconds);
    const auto timeout_for_client = std::chrono::duration<double>(timeout);
    auto client = std::make_shared<rclcpp::SyncParametersClient>(
        this, std::string(cmd.node_name));
    if (!client->wait_for_service(timeout)) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_DESCRIBE_FAILED",
                "Parameter services unavailable before timeout");
      continue;
    }

    ParamDescribeResponseCmd response(allocator_);
    response.request_id = cmd.request_id;
    response.node_name = cmd.node_name;
    response.param_name = cmd.param_name;

    std::vector<rcl_interfaces::msg::ParameterDescriptor> descriptors;
    try {
      descriptors = client->describe_parameters({std::string(cmd.param_name)},
                                                timeout_for_client);
    } catch (const std::exception& ex) {
      SendError(cmd.request_id, "PARAM_DESCRIBE_FAILED", ex.what());
      continue;
    }

    if (!descriptors.empty()) {
      const auto& descriptor = descriptors.front();
      response.found = true;
      response.param_type = rclcpp::to_string(
          static_cast<rclcpp::ParameterType>(descriptor.type));
      response.description = descriptor.description;
      response.read_only = descriptor.read_only;
      response.constraints =
          BuildDescriptorConstraintsText(descriptor, allocator_);
    }

    out_storage.Enqueue(param_describe_response_producer_, std::move(response));
  }
}

void ParamDescribeNode::OnPollTimer() {
  DrainParamDescribeCommands();
}

void ParamDescribeNode::SendError(uint64_t request_id,
                                  std::string_view error_code,
                                  std::string_view error_message) {
  ErrorCmd cmd(allocator_);
  cmd.request_id = request_id;
  cmd.error_code = std::pmr::string(error_code, allocator_);
  cmd.error_message = std::pmr::string(error_message, allocator_);

  outgoing_.get().Enqueue(error_producer_, std::move(cmd));
}

}  // namespace roscraft::bridge
