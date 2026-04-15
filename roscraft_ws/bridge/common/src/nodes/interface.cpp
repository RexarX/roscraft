#include <pch.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/nodes/details/graph_utils.hpp>
#include <roscraft/bridge/nodes/interface.hpp>

#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <memory_resource>
#include <string_view>

namespace roscraft::bridge {

InterfaceNode::InterfaceNode(CommandQueue& incoming, CommandQueue& outgoing)
    : rclcpp::Node("roscraft_interface_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      interface_list_consumer_(incoming.MakeConsumerToken<InterfaceListCmd>()),
      interface_show_consumer_(incoming.MakeConsumerToken<InterfaceShowCmd>()),
      interface_list_response_producer_(
          outgoing.MakeProducerToken<InterfaceListResponseCmd>()),
      interface_show_response_producer_(
          outgoing.MakeProducerToken<InterfaceShowResponseCmd>()) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(100ms, [this] { OnPollTimer(); });
}

void InterfaceNode::DrainInterfaceListCommands() {
  auto& in_storage = incoming_.get().TypedStorage<InterfaceListCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<InterfaceListResponseCmd>();

  InterfaceListCmd cmd;
  while (in_storage.Dequeue(interface_list_consumer_, cmd)) {
    InterfaceListResponseCmd response(std::pmr::get_default_resource());
    response.request_id = cmd.request_id;

    if (cmd.include_messages) {
      const auto messages = details::BuildTypeListFromResources("msg", "msg");
      response.messages.reserve(messages.size());
      for (const auto& value : messages) {
        response.messages.emplace_back(value);
      }
    }

    if (cmd.include_services) {
      const auto services = details::BuildTypeListFromResources("srv", "srv");
      response.services.reserve(services.size());
      for (const auto& value : services) {
        response.services.emplace_back(value);
      }
    }

    if (cmd.include_actions) {
      const auto actions =
          details::BuildTypeListFromResources("action", "action");
      response.actions.reserve(actions.size());
      for (const auto& value : actions) {
        response.actions.emplace_back(value);
      }
    }

    out_storage.Enqueue(interface_list_response_producer_, std::move(response));
  }
}

void InterfaceNode::DrainInterfaceShowCommands() {
  auto& in_storage = incoming_.get().TypedStorage<InterfaceShowCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<InterfaceShowResponseCmd>();

  InterfaceShowCmd cmd(std::pmr::get_default_resource());
  while (in_storage.Dequeue(interface_show_consumer_, cmd)) {
    InterfaceShowResponseCmd response(std::pmr::get_default_resource());
    response.request_id = cmd.request_id;
    response.interface_type = cmd.interface_type;

    const auto parsed = details::ParseInterfaceType(cmd.interface_type);
    if (parsed.has_value()) [[likely]] {
      const auto definition = details::ReadInterfaceDefinition(*parsed);
      if (definition.has_value()) [[likely]] {
        response.found = true;
        response.definition = *definition;
      }
    }

    out_storage.Enqueue(interface_show_response_producer_, std::move(response));
  }
}

void InterfaceNode::OnPollTimer() {
  DrainInterfaceListCommands();
  DrainInterfaceShowCommands();
}

}  // namespace roscraft::bridge
