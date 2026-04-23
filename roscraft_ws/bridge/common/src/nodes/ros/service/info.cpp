#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/service/info.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/service.hpp>
#include <roscraft/bridge/nodes/ros/details/graph_utils.hpp>

#include <rclcpp/rclcpp.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory_resource>
#include <string>

namespace roscraft::bridge {

ServiceInfoNode::ServiceInfoNode(CommandQueue& incoming, CommandQueue& outgoing,
                                 std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_service_info_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      service_info_consumer_(incoming.MakeConsumerToken<ServiceInfoCmd>()),
      service_info_response_producer_(
          outgoing.MakeProducerToken<ServiceInfoResponseCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void ServiceInfoNode::DrainServiceInfoCommands() {
  auto& in_storage = incoming_.get().TypedStorage<ServiceInfoCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<ServiceInfoResponseCmd>();

  std::string service_name;

  ServiceInfoCmd cmd(allocator_);
  while (in_storage.Dequeue(service_info_consumer_, cmd)) {
    ServiceInfoResponseCmd response(allocator_);
    response.request_id = cmd.request_id;
    response.service_name = cmd.service_name;

    service_name.assign(cmd.service_name);

    const auto service_map = this->get_service_names_and_types();
    const auto it = service_map.find(service_name);
    if (it != service_map.end()) {
      const auto& types = it->second;
      if (!types.empty()) {
        response.service_type = types.front();
      }
    }

    response.client_count =
        static_cast<uint32_t>(this->count_clients(service_name));
    response.server_count =
        static_cast<uint32_t>(this->count_services(service_name));

    const auto node_graph = this->get_node_graph_interface();
    const auto discovered_nodes = node_graph->get_node_names_and_namespaces();

    for (const auto& [node_name, node_namespace] : discovered_nodes) {
      const auto server_map =
          this->get_service_names_and_types_by_node(node_name, node_namespace);
      if (server_map.contains(service_name)) {
        response.server_nodes.emplace_back(
            details::BuildFullyQualifiedNodeName(node_name, node_namespace));
      }

      const auto client_map = node_graph->get_client_names_and_types_by_node(
          node_name, node_namespace);
      if (client_map.contains(service_name)) {
        response.client_nodes.emplace_back(
            details::BuildFullyQualifiedNodeName(node_name, node_namespace));
      }
    }

    std::ranges::sort(response.client_nodes);
    const auto unique_client_nodes_begin =
        std::ranges::unique(response.client_nodes).begin();
    response.client_nodes.erase(unique_client_nodes_begin,
                                response.client_nodes.end());

    std::ranges::sort(response.server_nodes);
    const auto unique_server_nodes_begin =
        std::ranges::unique(response.server_nodes).begin();
    response.server_nodes.erase(unique_server_nodes_begin,
                                response.server_nodes.end());

    out_storage.Enqueue(service_info_response_producer_, std::move(response));
  }
}

void ServiceInfoNode::OnPollTimer() {
  DrainServiceInfoCommands();
}

}  // namespace roscraft::bridge
