#include <pch.hpp>

#include <roscraft/bridge/nodes/graph.hpp>

#include <roscraft/bridge/command/queue.hpp>

#include <algorithm>
#include <chrono>
#include <memory_resource>
#include <string_view>

namespace roscraft::bridge {

void GraphNode::RefreshSnapshot() {
  auto* pending_topics = &topics_a_;
  auto* pending_services = &services_a_;
  auto* pending_actions = &actions_a_;
  if (PendingSnapshotIndex() == 1UL) {
    pending_topics = &topics_b_;
    pending_services = &services_b_;
    pending_actions = &actions_b_;
  }

  // ---- topics ----
  {
    const auto topic_map = this->get_topic_names_and_types();
    pending_topics->clear();
    pending_topics->reserve(topic_map.size());
    for (const auto& [name, _] : topic_map) {
      pending_topics->push_back(name);
    }
    std::ranges::sort(*pending_topics);
  }

  // ---- services ----
  {
    const auto svc_map = this->get_service_names_and_types();
    pending_services->clear();
    pending_services->reserve(svc_map.size());
    for (const auto& [name, _] : svc_map) {
      pending_services->push_back(name);
    }
    std::ranges::sort(*pending_services);
  }

  // ---- actions ----
  // ROS2 actions are built on top of topics/services; identify them by the
  // canonical "/_action/..." suffix pattern.
  {
    pending_actions->clear();
    for (const auto& topic : *pending_topics) {
      // Action topics share the base name: strip the "/_action/..." tail.
      static constexpr std::string_view kSuffix = "/_action/status";
      if (topic.size() > kSuffix.size() && topic.ends_with(kSuffix)) {
        pending_actions->push_back(
            topic.substr(0, topic.size() - kSuffix.size()));
      }
    }
    std::ranges::sort(*pending_actions);
    pending_actions->erase(std::ranges::unique(*pending_actions).begin(),
                           pending_actions->end());
  }

  current_snapshot_index_.store(PendingSnapshotIndex(), std::memory_order_release);
}

void GraphNode::DrainAndRespond() {
  auto& in_storage = incoming_.get().TypedStorage<QueryGraphCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<GraphSnapshotCmd>();

  const auto index = CurrentSnapshotIndex();
  const auto& cached_topics = index == 0UL ? topics_a_ : topics_b_;
  const auto& cached_services = index == 0UL ? services_a_ : services_b_;
  const auto& cached_actions = index == 0UL ? actions_a_ : actions_b_;

  QueryGraphCmd query;
  while (in_storage.Dequeue(query_consumer_, query)) {
    // Build a GraphSnapshotCmd from the current cache.
    // Use the default PMR resource (new/delete) since these commands are
    // enqueued and will outlive the arena.
    GraphSnapshotCmd snap(std::pmr::get_default_resource());
    snap.request_id = query.request_id;
    snap.topics.reserve(cached_topics.size());
    snap.services.reserve(cached_services.size());
    snap.actions.reserve(cached_actions.size());

    for (const auto& topic : cached_topics) {
      snap.topics.emplace_back(topic);
    }
    for (const auto& service : cached_services) {
      snap.services.emplace_back(service);
    }
    for (const auto& action : cached_actions) {
      snap.actions.emplace_back(action);
    }

    out_storage.Enqueue(snapshot_producer_, std::move(snap));
  }
}

void GraphNode::OnPollTimer() {
  RefreshSnapshot();
  DrainAndRespond();
}

void GraphNode::WatcherTaskFunc() {
  using namespace std::chrono_literals;

  while (!stop_watcher_.load(std::memory_order_acquire)) {
    // Block until the graph changes or the event is signalled for shutdown.
    // 200 ms timeout so we don't miss the stop flag if the event is never set.
    wait_for_graph_change(graph_event_, 200ms);

    if (stop_watcher_.load(std::memory_order_acquire)) {
      break;
    }

    // Re-arm the event so the next change wakes us again.
    graph_event_ = get_graph_event();

    // Post the refresh back onto the ROS executor's thread so that
    // RefreshSnapshot and DrainAndRespond are always called from the spin
    // thread — avoiding any data races with the poll timer callbacks.
    // We use a zero-duration one-shot wall timer as a "post to spin thread"
    // primitive: the executor fires it on its very next iteration.
    {
      std::scoped_lock guard(post_mutex_);
      if (!pending_graph_refresh_) {
        pending_graph_refresh_ = true;
        // Zero-duration timer fires on the very next executor iteration.
        refresh_post_timer_ =
            create_wall_timer(1ns, [this] { OnGraphRefreshPost(); });
      }
    }
  }
}

void GraphNode::OnGraphRefreshPost() {
  // Cancel the one-shot timer so it doesn't repeat.
  refresh_post_timer_->cancel();
  refresh_post_timer_.reset();
  {
    std::scoped_lock guard(post_mutex_);
    pending_graph_refresh_ = false;
  }

  RefreshSnapshot();
  DrainAndRespond();
}

}  // namespace roscraft::bridge
