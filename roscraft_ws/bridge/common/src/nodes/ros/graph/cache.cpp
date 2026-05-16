#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/graph/cache.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/graph.hpp>
#include <roscraft/memory/arena_allocator.hpp>

#include <rclcpp/rclcpp.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <memory_resource>
#include <string>
#include <string_view>
#include <unordered_map>

namespace roscraft::bridge {

GraphCacheNode::GraphCacheNode(CommandQueue& incoming, CommandQueue& outgoing,
                               std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_graph_cache_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      query_consumer_(incoming.MakeConsumerToken<QueryGraphCmd>()),
      snapshot_producer_(outgoing.MakeProducerToken<GraphSnapshotCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;

  poll_timer_ = this->create_wall_timer(250ms, [this] { OnPollTimer(); });
  graph_event_ = this->get_graph_event();

  RefreshSnapshot();

  watcher_thread_ = std::jthread(
      [this](std::stop_token stoken) { WatcherTaskFunc(std::move(stoken)); });
}

GraphCacheNode::~GraphCacheNode() {
  stop_watcher_.store(true, std::memory_order_release);
  if (refresh_post_timer_ != nullptr) {
    refresh_post_timer_->cancel();
  }
  if (graph_event_ != nullptr) {
    graph_event_->set();
  }
  watcher_thread_.request_stop();
  if (watcher_thread_.joinable()) {
    watcher_thread_.join();
  }
}

void GraphCacheNode::RefreshSnapshot() {
  auto& [topics, services, actions] = PendingSnapshot();

  {
    const auto topic_map = this->get_topic_names_and_types();
    topics.clear();
    topics.reserve(topic_map.size());
    for (const auto& [name, types] : topic_map) {
      for (const auto& type : types) {
        topics.emplace_back(name, type);
      }
    }

    std::ranges::sort(topics, [](const auto& lhs, const auto& rhs) {
      return lhs.name < rhs.name;
    });
  }

  {
    const auto svc_map = this->get_service_names_and_types();
    services.clear();
    services.reserve(svc_map.size());
    for (const auto& [name, types] : svc_map) {
      for (const auto& type : types) {
        services.emplace_back(name, type);
      }
    }

    std::ranges::sort(services, [](const auto& lhs, const auto& rhs) {
      return lhs.name < rhs.name;
    });
  }

  {
    std::pmr::unordered_map<std::string_view, std::string_view>
        topic_type_lookup{&scratch_arena_};
    topic_type_lookup.reserve(topics.size());
    for (const auto& topic : topics) {
      if (!topic.name.empty() && !topic_type_lookup.contains(topic.name)) {
        topic_type_lookup[topic.name] = topic.type;
      }
    }

    static constexpr std::string_view kStatusSuffix = "/_action/status";
    static constexpr std::string_view kFeedbackSuffix = "/_action/feedback";
    static constexpr std::string_view kFeedbackTypeSuffix = "_Feedback";

    actions.clear();
    for (const auto& topic : topics) {
      if (topic.name.size() > kStatusSuffix.size() &&
          topic.name.ends_with(kStatusSuffix)) {
        std::string action_name =
            topic.name.substr(0, topic.name.size() - kStatusSuffix.size());
        std::string action_type;

        std::pmr::string feedback_topic{&scratch_arena_};
        const size_t formatted_size =
            std::formatted_size("{}{}", action_name, kFeedbackSuffix);
        feedback_topic.resize(formatted_size);
        std::format_to(feedback_topic.begin(), "{}{}", action_name,
                       kFeedbackSuffix);

        if (const auto it = topic_type_lookup.find(feedback_topic);
            it != topic_type_lookup.end() &&
            it->second.ends_with(kFeedbackTypeSuffix)) {
          action_type.assign(it->second.begin(),
                             it->second.end() - kFeedbackTypeSuffix.size());
        }

        actions.emplace_back(std::move(action_name), std::move(action_type));
      }
    }

    std::ranges::sort(actions, [](const auto& lhs, const auto& rhs) {
      return lhs.name < rhs.name;
    });

    actions.erase(std::ranges::unique(actions,
                                      [](const auto& lhs, const auto& rhs) {
                                        return lhs.name == rhs.name;
                                      })
                      .begin(),
                  actions.end());
  }

  current_snapshot_index_.store(PendingSnapshotIndex(),
                                std::memory_order_release);
}

void GraphCacheNode::DrainAndRespond() {
  auto& in_storage = incoming_.get().TypedStorage<QueryGraphCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<GraphSnapshotCmd>();

  const auto& [topics, services, actions] = CurrentSnapshot();
  const auto node_names = this->get_node_names();

  QueryGraphCmd query;
  while (in_storage.Dequeue(query_consumer_, query)) {
    GraphSnapshotCmd snap(allocator_);
    snap.request_id = query.request_id;
    snap.topics.reserve(topics.size());
    snap.services.reserve(services.size());
    snap.actions.reserve(actions.size());
    snap.nodes.reserve(node_names.size());

    for (const auto& topic : topics) {
      auto& entry =
          snap.topics.emplace_back(snap.topics.get_allocator().resource());
      entry.name = topic.name;
      entry.type = topic.type;
    }

    for (const auto& service : services) {
      auto& entry =
          snap.services.emplace_back(snap.services.get_allocator().resource());
      entry.name = service.name;
      entry.type = service.type;
    }

    for (const auto& action : actions) {
      auto& entry =
          snap.actions.emplace_back(snap.actions.get_allocator().resource());
      entry.name = action.name;
      entry.type = action.type;
    }

    for (const auto& node_name : node_names) {
      auto& entry =
          snap.nodes.emplace_back(snap.nodes.get_allocator().resource());
      entry.name = node_name;
    }

    out_storage.Enqueue(snapshot_producer_, std::move(snap));
  }
}

void GraphCacheNode::OnPollTimer() {
  RefreshSnapshot();
  DrainAndRespond();
  scratch_arena_.Reset();
}

void GraphCacheNode::OnGraphRefreshPost() {
  pending_graph_refresh_.store(false, std::memory_order_release);

  RefreshSnapshot();
  DrainAndRespond();
  scratch_arena_.Reset();
}

void GraphCacheNode::WatcherTaskFunc(std::stop_token stoken) {
  using namespace std::chrono_literals;

  while (!stoken.stop_requested() &&
         !stop_watcher_.load(std::memory_order_acquire)) {
    this->wait_for_graph_change(graph_event_, 200ms);

    if (stoken.stop_requested() ||
        stop_watcher_.load(std::memory_order_acquire)) {
      break;
    }

    if (!pending_graph_refresh_.load(std::memory_order_acquire)) {
      pending_graph_refresh_.store(true, std::memory_order_release);
      graph_event_ = this->get_graph_event();

      if (refresh_post_timer_ != nullptr) {
        refresh_post_timer_->cancel();
      }
      refresh_post_timer_ = this->create_wall_timer(1ns, [this] {
        refresh_post_timer_->cancel();
        OnGraphRefreshPost();
      });
    }
  }
}

}  // namespace roscraft::bridge
