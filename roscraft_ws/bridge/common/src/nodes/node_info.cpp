#include <pch.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/nodes/details/graph_utils.hpp>
#include <roscraft/bridge/nodes/node_info.hpp>

#include <rclcpp/rclcpp.hpp>

#include <algorithm>
#include <chrono>
#include <memory_resource>

namespace roscraft::bridge {

NodeInfoNode::NodeInfoNode(CommandQueue& incoming, CommandQueue& outgoing)
    : rclcpp::Node("roscraft_node_info_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      node_info_consumer_(incoming.MakeConsumerToken<NodeInfoCmd>()),
      node_info_response_producer_(
          outgoing.MakeProducerToken<NodeInfoResponseCmd>()) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(100ms, [this] { OnPollTimer(); });
}

void NodeInfoNode::DrainNodeInfoCommands() {
  auto& in_storage = incoming_.get().TypedStorage<NodeInfoCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<NodeInfoResponseCmd>();

  NodeInfoCmd cmd(std::pmr::get_default_resource());
  while (in_storage.Dequeue(node_info_consumer_, cmd)) {
    NodeInfoResponseCmd response(std::pmr::get_default_resource());
    response.request_id = cmd.request_id;
    response.node_name = cmd.node_name;

    const auto node_graph = this->get_node_graph_interface();
    const auto canonical =
        details::CanonicalizeNodeNameAndNamespace(cmd.node_name);
    if (!canonical.has_value()) {
      out_storage.Enqueue(node_info_response_producer_, std::move(response));
      continue;
    }

    const auto& [node_name, node_namespace] = *canonical;

    bool found = false;
    const auto discovered_nodes = node_graph->get_node_names_and_namespaces();
    for (const auto& [name, node_ns] : discovered_nodes) {
      if (name == node_name && node_ns == node_namespace) {
        found = true;
        break;
      }
    }

    if (!found) {
      out_storage.Enqueue(node_info_response_producer_, std::move(response));
      continue;
    }

    response.found = true;

    const auto publisher_map =
        node_graph->get_publisher_names_and_types_by_node(node_name,
                                                          node_namespace);
    for (const auto& [topic_name, topic_types] : publisher_map) {
      if (!cmd.include_hidden && details::IsHiddenName(topic_name)) {
        continue;
      }
      for (const auto& topic_type : topic_types) {
        auto& entry = response.publishers.emplace_back(
            response.publishers.get_allocator().resource());
        entry.name = topic_name;
        entry.type = topic_type;
      }
    }

    const auto subscriber_map =
        node_graph->get_subscriber_names_and_types_by_node(node_name,
                                                           node_namespace);
    for (const auto& [topic_name, topic_types] : subscriber_map) {
      if (!cmd.include_hidden && details::IsHiddenName(topic_name)) {
        continue;
      }
      for (const auto& topic_type : topic_types) {
        auto& entry = response.subscribers.emplace_back(
            response.subscribers.get_allocator().resource());
        entry.name = topic_name;
        entry.type = topic_type;
      }
    }

    const auto service_map =
        this->get_service_names_and_types_by_node(node_name, node_namespace);
    for (const auto& [service_name, service_types] : service_map) {
      if (!cmd.include_hidden && details::IsHiddenName(service_name)) {
        continue;
      }
      for (const auto& service_type : service_types) {
        auto& entry = response.services.emplace_back(
            response.services.get_allocator().resource());
        entry.name = service_name;
        entry.type = service_type;
      }
    }

    const auto client_map = node_graph->get_client_names_and_types_by_node(
        node_name, node_namespace);
    for (const auto& [service_name, service_types] : client_map) {
      if (!cmd.include_hidden && details::IsHiddenName(service_name)) {
        continue;
      }
      for (const auto& service_type : service_types) {
        auto& entry = response.services.emplace_back(
            response.services.get_allocator().resource());
        entry.name = service_name;
        entry.type = service_type;
      }
    }

    const auto by_name_and_type = [](const auto& lhs, const auto& rhs) {
      if (lhs.name == rhs.name) {
        return lhs.type < rhs.type;
      }
      return lhs.name < rhs.name;
    };
    const auto same_name_and_type = [](const auto& lhs, const auto& rhs) {
      return lhs.name == rhs.name && lhs.type == rhs.type;
    };

    std::ranges::sort(response.publishers, by_name_and_type);
    response.publishers.erase(
        std::ranges::unique(response.publishers, same_name_and_type).begin(),
        response.publishers.end());

    std::ranges::sort(response.subscribers, by_name_and_type);
    response.subscribers.erase(
        std::ranges::unique(response.subscribers, same_name_and_type).begin(),
        response.subscribers.end());

    std::ranges::sort(response.services, by_name_and_type);
    response.services.erase(
        std::ranges::unique(response.services, same_name_and_type).begin(),
        response.services.end());

    out_storage.Enqueue(node_info_response_producer_, std::move(response));
  }
}

void NodeInfoNode::OnPollTimer() {
  DrainNodeInfoCommands();
}

}  // namespace roscraft::bridge
