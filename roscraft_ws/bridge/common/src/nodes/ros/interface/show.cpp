#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/interface/show.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/interface.hpp>
#include <roscraft/bridge/nodes/ros/details/graph_utils.hpp>

#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <memory_resource>

namespace roscraft::bridge {

InterfaceShowNode::InterfaceShowNode(CommandQueue& incoming,
                                     CommandQueue& outgoing,
                                     std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_interface_show_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      interface_show_consumer_(incoming.MakeConsumerToken<InterfaceShowCmd>()),
      interface_show_response_producer_(
          outgoing.MakeProducerToken<InterfaceShowResponseCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void InterfaceShowNode::DrainInterfaceShowCommands() {
  auto& in_storage = incoming_.get().TypedStorage<InterfaceShowCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<InterfaceShowResponseCmd>();

  InterfaceShowCmd cmd(allocator_);
  while (in_storage.Dequeue(interface_show_consumer_, cmd)) {
    InterfaceShowResponseCmd response(allocator_);
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

void InterfaceShowNode::OnPollTimer() {
  DrainInterfaceShowCommands();
}

}  // namespace roscraft::bridge
