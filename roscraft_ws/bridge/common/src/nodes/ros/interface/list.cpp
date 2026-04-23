#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/interface/list.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/interface.hpp>
#include <roscraft/bridge/nodes/ros/details/graph_utils.hpp>

#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <memory_resource>

namespace roscraft::bridge {

InterfaceListNode::InterfaceListNode(CommandQueue& incoming,
                                     CommandQueue& outgoing,
                                     std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_interface_list_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      interface_list_consumer_(incoming.MakeConsumerToken<InterfaceListCmd>()),
      interface_list_response_producer_(
          outgoing.MakeProducerToken<InterfaceListResponseCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void InterfaceListNode::DrainInterfaceListCommands() {
  auto& in_storage = incoming_.get().TypedStorage<InterfaceListCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<InterfaceListResponseCmd>();

  InterfaceListCmd cmd;
  while (in_storage.Dequeue(interface_list_consumer_, cmd)) {
    InterfaceListResponseCmd response(allocator_);
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

void InterfaceListNode::OnPollTimer() {
  DrainInterfaceListCommands();
}

}  // namespace roscraft::bridge
