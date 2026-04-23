#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/param/get.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/error.hpp>
#include <roscraft/bridge/command/types/param.hpp>
#include <roscraft/bridge/nodes/ros/common.hpp>

#include <rclcpp/parameter_client.hpp>
#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

ParamGetNode::ParamGetNode(CommandQueue& incoming, CommandQueue& outgoing,
                           std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_param_get_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      param_get_consumer_(incoming.MakeConsumerToken<ParamGetCmd>()),
      param_get_response_producer_(
          outgoing.MakeProducerToken<ParamGetResponseCmd>()),
      error_producer_(outgoing.MakeProducerToken<ErrorCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void ParamGetNode::DrainParamGetCommands() {
  auto& in_storage = incoming_.get().TypedStorage<ParamGetCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<ParamGetResponseCmd>();

  ParamGetCmd cmd(allocator_);
  while (in_storage.Dequeue(param_get_consumer_, cmd)) {
    if (cmd.node_name.empty() || cmd.param_name.empty()) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_GET_FAILED",
                "node_name and param_name must be non-empty");
      continue;
    }

    const auto timeout = ResolveTimeout(cmd.timeout_seconds);
    const auto timeout_for_client = std::chrono::duration<double>(timeout);
    auto client = std::make_shared<rclcpp::SyncParametersClient>(
        this, std::string(cmd.node_name));
    if (!client->wait_for_service(timeout)) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_GET_FAILED",
                "Parameter services unavailable before timeout");
      continue;
    }

    ParamGetResponseCmd response(allocator_);
    response.request_id = cmd.request_id;
    response.node_name = cmd.node_name;
    response.param_name = cmd.param_name;
    response.type_hidden = cmd.hide_type;

    std::vector<rclcpp::Parameter> parameters;
    try {
      parameters = client->get_parameters({std::string(cmd.param_name)},
                                          timeout_for_client);
    } catch (const std::exception& ex) {
      SendError(cmd.request_id, "PARAM_GET_FAILED", ex.what());
      continue;
    }

    if (!parameters.empty() && parameters.front().get_type() !=
                                   rclcpp::ParameterType::PARAMETER_NOT_SET) {
      response.found = true;
      response.value_text = parameters.front().value_to_string();
      if (!cmd.hide_type) {
        response.param_type = rclcpp::to_string(parameters.front().get_type());
      }
    }

    out_storage.Enqueue(param_get_response_producer_, std::move(response));
  }
}

void ParamGetNode::OnPollTimer() {
  DrainParamGetCommands();
}

void ParamGetNode::SendError(uint64_t request_id, std::string_view error_code,
                             std::string_view error_message) {
  ErrorCmd cmd(allocator_);
  cmd.request_id = request_id;
  cmd.error_code = std::pmr::string(error_code, allocator_);
  cmd.error_message = std::pmr::string(error_message, allocator_);

  outgoing_.get().Enqueue(error_producer_, std::move(cmd));
}

}  // namespace roscraft::bridge
