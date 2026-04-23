#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/param/list.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/error.hpp>
#include <roscraft/bridge/command/types/param.hpp>
#include <roscraft/bridge/nodes/ros/common.hpp>

#include <rclcpp/parameter_client.hpp>
#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <memory_resource>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

ParamListNode::ParamListNode(CommandQueue& incoming, CommandQueue& outgoing,
                             std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_param_list_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      param_list_consumer_(incoming.MakeConsumerToken<ParamListCmd>()),
      param_list_response_producer_(
          outgoing.MakeProducerToken<ParamListResponseCmd>()),
      error_producer_(outgoing.MakeProducerToken<ErrorCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void ParamListNode::DrainParamListCommands() {
  auto& in_storage = incoming_.get().TypedStorage<ParamListCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<ParamListResponseCmd>();

  ParamListCmd cmd(allocator_);
  while (in_storage.Dequeue(param_list_consumer_, cmd)) {
    if (cmd.node_name.empty()) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_LIST_FAILED",
                "node_name must be non-empty");
      continue;
    }

    const auto timeout = ResolveTimeout(cmd.timeout_seconds);
    const auto timeout_for_client = std::chrono::duration<double>(timeout);
    auto client = std::make_shared<rclcpp::SyncParametersClient>(
        this, std::string(cmd.node_name));
    if (!client->wait_for_service(timeout)) [[unlikely]] {
      SendError(cmd.request_id, "PARAM_LIST_FAILED",
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
      listed = client->list_parameters(prefixes, cmd.depth, timeout_for_client);
    } catch (const std::exception& ex) {
      SendError(cmd.request_id, "PARAM_LIST_FAILED", ex.what());
      continue;
    }

    ParamListResponseCmd response(allocator_);
    response.request_id = cmd.request_id;
    response.node_name = cmd.node_name;

    std::optional<std::regex> name_filter;
    if (!cmd.filter_regex.empty()) {
      try {
        name_filter.emplace(std::string(cmd.filter_regex),
                            std::regex::ECMAScript | std::regex::optimize);
      } catch (const std::exception& ex) {
        SendError(cmd.request_id, "PARAM_LIST_FAILED",
                  std::format("Invalid filter regex: {}", ex.what()));
        continue;
      }
    }

    std::vector<std::string> filtered_names;
    filtered_names.reserve(listed.names.size());
    for (const auto& name : listed.names) {
      if (name_filter.has_value() && !std::regex_search(name, *name_filter)) {
        continue;
      }

      response.names.emplace_back(name);
      filtered_names.emplace_back(name);
    }

    response.prefixes.reserve(listed.prefixes.size());
    for (const auto& prefix : listed.prefixes) {
      response.prefixes.emplace_back(prefix);
    }

    if (cmd.include_types && !filtered_names.empty()) {
      std::vector<rclcpp::ParameterType> types;
      try {
        types = client->get_parameter_types(filtered_names, timeout_for_client);
      } catch (const std::exception& ex) {
        SendError(cmd.request_id, "PARAM_LIST_FAILED", ex.what());
        continue;
      }

      response.types.reserve(types.size());
      for (const auto type : types) {
        response.types.emplace_back(rclcpp::to_string(type));
      }
    }

    out_storage.Enqueue(param_list_response_producer_, std::move(response));
  }
}

void ParamListNode::OnPollTimer() {
  DrainParamListCommands();
}

void ParamListNode::SendError(uint64_t request_id, std::string_view error_code,
                              std::string_view error_message) {
  ErrorCmd cmd(allocator_);
  cmd.request_id = request_id;
  cmd.error_code = std::pmr::string(error_code, allocator_);
  cmd.error_message = std::pmr::string(error_message, allocator_);

  outgoing_.get().Enqueue(error_producer_, std::move(cmd));
}

}  // namespace roscraft::bridge
