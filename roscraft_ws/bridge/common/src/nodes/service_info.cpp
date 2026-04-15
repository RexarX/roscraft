#include <pch.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/nodes/details/graph_utils.hpp>
#include <roscraft/bridge/nodes/service_info.hpp>

#include <rclcpp/rclcpp.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory_resource>
#include <string>

namespace roscraft::bridge {

ServiceInfoNode::ServiceInfoNode(CommandQueue& incoming, CommandQueue& outgoing)
    : rclcpp::Node("roscraft_service_info_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      service_info_consumer_(incoming.MakeConsumerToken<ServiceInfoCmd>()),
      service_info_response_producer_(
          outgoing.MakeProducerToken<ServiceInfoResponseCmd>()) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(100ms, [this] { OnPollTimer(); });
}

void ServiceInfoNode::DrainServiceInfoCommands() {
  auto& in_storage = incoming_.get().TypedStorage<ServiceInfoCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<ServiceInfoResponseCmd>();

  std::string service_name;

  ServiceInfoCmd cmd(std::pmr::get_default_resource());
  while (in_storage.Dequeue(service_info_consumer_, cmd)) {
    ServiceInfoResponseCmd response(std::pmr::get_default_resource());
    response.request_id = cmd.request_id;
    response.service_name = cmd.service_name;

    service_name.assign(cmd.service_name);

    if (const auto service_map = this->get_service_names_and_types();
        service_map.contains(service_name)) {
      const auto& types = service_map.at(service_name);
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
      if (const auto server_map = this->get_service_names_and_types_by_node(
              node_name, node_namespace);
          server_map.contains(service_name)) {
        response.server_nodes.emplace_back(
            details::BuildFullyQualifiedNodeName(node_name, node_namespace));
      }

      if (const auto client_map =
              node_graph->get_client_names_and_types_by_node(node_name,
                                                             node_namespace);
          client_map.contains(service_name)) {
        response.client_nodes.emplace_back(
            details::BuildFullyQualifiedNodeName(node_name, node_namespace));
      }
    }

    std::ranges::sort(response.client_nodes);
    response.client_nodes.erase(
        std::ranges::unique(response.client_nodes).begin(),
        response.client_nodes.end());

    std::ranges::sort(response.server_nodes);
    response.server_nodes.erase(
        std::ranges::unique(response.server_nodes).begin(),
        response.server_nodes.end());

    out_storage.Enqueue(service_info_response_producer_, std::move(response));
  }
}

void ServiceInfoNode::OnPollTimer() {
  DrainServiceInfoCommands();
}

}  // namespace roscraft::bridge
