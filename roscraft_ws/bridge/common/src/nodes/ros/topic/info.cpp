#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/topic/info.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/topic.hpp>
#include <roscraft/bridge/nodes/ros/details/graph_utils.hpp>

#include <rclcpp/rclcpp.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory_resource>
#include <string>

namespace roscraft::bridge {

TopicInfoNode::TopicInfoNode(CommandQueue& incoming, CommandQueue& outgoing,
                             std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_topic_info_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      topic_info_consumer_(incoming.MakeConsumerToken<TopicInfoCmd>()),
      topic_info_response_producer_(
          outgoing.MakeProducerToken<TopicInfoResponseCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void TopicInfoNode::DrainTopicInfoCommands() {
  auto& in_storage = incoming_.get().TypedStorage<TopicInfoCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<TopicInfoResponseCmd>();

  TopicInfoCmd cmd(allocator_);
  while (in_storage.Dequeue(topic_info_consumer_, cmd)) {
    TopicInfoResponseCmd response(allocator_);
    response.request_id = cmd.request_id;
    response.topic_name = cmd.topic_name;

    const std::string topic_name(cmd.topic_name);

    const auto topic_map = this->get_topic_names_and_types();
    if (const auto it = topic_map.find(topic_name); it != topic_map.end()) {
      const auto& types = it->second;
      if (!types.empty()) {
        response.message_type = types.front();
      }
    }

    response.publisher_count =
        static_cast<uint32_t>(this->count_publishers(topic_name));
    response.subscriber_count =
        static_cast<uint32_t>(this->count_subscribers(topic_name));

    const auto node_graph = this->get_node_graph_interface();
    const auto discovered_nodes = node_graph->get_node_names_and_namespaces();

    for (const auto& [node_name, node_namespace] : discovered_nodes) {
      const auto publisher_map =
          node_graph->get_publisher_names_and_types_by_node(node_name,
                                                            node_namespace);
      if (publisher_map.contains(topic_name)) {
        response.publisher_nodes.emplace_back(
            details::BuildFullyQualifiedNodeName(node_name, node_namespace));
      }

      const auto subscriber_map =
          node_graph->get_subscriber_names_and_types_by_node(node_name,
                                                             node_namespace);
      if (subscriber_map.contains(topic_name)) {
        response.subscriber_nodes.emplace_back(
            details::BuildFullyQualifiedNodeName(node_name, node_namespace));
      }
    }

    std::ranges::sort(response.publisher_nodes);
    const auto unique_publisher_nodes =
        std::ranges::unique(response.publisher_nodes);
    response.publisher_nodes.erase(unique_publisher_nodes.begin(),
                                   response.publisher_nodes.end());

    std::ranges::sort(response.subscriber_nodes);
    const auto unique_subscriber_nodes =
        std::ranges::unique(response.subscriber_nodes);
    response.subscriber_nodes.erase(unique_subscriber_nodes.begin(),
                                    response.subscriber_nodes.end());

    out_storage.Enqueue(topic_info_response_producer_, std::move(response));
  }
}

void TopicInfoNode::OnPollTimer() {
  DrainTopicInfoCommands();
}

}  // namespace roscraft::bridge
