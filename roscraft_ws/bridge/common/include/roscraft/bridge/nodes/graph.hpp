#pragma once

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/event.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <taskflow/taskflow.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <vector>

namespace roscraft::bridge {

/// @brief Polls the ROS2 graph and fulfils pending `QueryGraphCmd` requests.
/// @details Uses a dedicated graph-watcher async task that calls
/// `wait_for_graph_change()` so topology changes trigger an immediate refresh,
/// with a 500 ms fallback timer in case any event is missed.
///
/// Thread model:
/// - Graph-watcher task: blocks on `wait_for_graph_change()`, posts a
///   refresh callback back to the ROS executor on each wakeup.
/// - ROS spin thread: runs `RefreshSnapshot()` and `DrainAndRespond()` via
///   the posted callback and the fallback timer.
class GraphNode final : public rclcpp::Node {
public:
  /// @brief Construct a GraphNode.
  /// @param incoming Incoming command queue (read `QueryGraphCmd`)
  /// @param outgoing Outgoing command queue (write `GraphSnapshotCmd`)
  /// @param executor Taskflow executor used for watcher task
  GraphNode(CommandQueue& incoming, CommandQueue& outgoing,
            tf::Executor& executor);
  ~GraphNode() override;

private:
  /// @brief Refresh the cached graph snapshot from live DDS discovery.
  void RefreshSnapshot();

  /// @brief Drain all pending `QueryGraphCmd`s and answer each with the cache.
  void DrainAndRespond();

  /// @brief Called on the poll timer tick — refresh then respond.
  void OnPollTimer();

  /// @brief Called on the spin thread after the watcher posts a graph change.
  void OnGraphRefreshPost();

  /// @brief Entry point for the graph-watcher background task.
  /// @details Loops calling `wait_for_graph_change()` until `stop_watcher_` is
  /// set, then posts a refresh callback to the ROS executor on each wakeup.
  void WatcherTaskFunc();

  /// @brief Returns inactive snapshot buffer index.
  [[nodiscard]] size_t PendingSnapshotIndex() const noexcept;

  /// @brief Returns active snapshot buffer index.
  [[nodiscard]] size_t CurrentSnapshotIndex() const noexcept;

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken query_consumer_;
  CommandQueueProducerToken snapshot_producer_;

  /// Fallback timer for periodic refresh (guards against missed graph events).
  rclcpp::TimerBase::SharedPtr poll_timer_;

  /// Graph event handle used by the watcher thread.
  rclcpp::Event::SharedPtr graph_event_;

  /// Taskflow async task that blocks on wait_for_graph_change().
  std::future<void> watcher_task_;

  /// Set to true to ask the watcher task to exit cleanly.
  std::atomic<bool> stop_watcher_{false};

  // ---- "post to spin thread" mechanism -------------------------------------
  // The watcher thread cannot safely call RefreshSnapshot() directly because
  // rclcpp callbacks and timer handlers all run on the ROS spin thread.
  // Instead the watcher arms a zero-duration one-shot timer; the executor
  // fires it on its next iteration, safely on the spin thread.

  /// Guards pending_graph_refresh_ and refresh_post_timer_.
  std::mutex post_mutex_;

  /// True while a refresh has been requested but not yet executed.
  /// Prevents stacking multiple redundant one-shot timers.
  bool pending_graph_refresh_{false};

  /// One-shot zero-duration timer used to marshal work to the spin thread.
  rclcpp::TimerBase::SharedPtr refresh_post_timer_;

  // ---- cached snapshot (double-buffered, no lock on ROS spin thread) -------

  std::vector<std::string> topics_a_;
  std::vector<std::string> services_a_;
  std::vector<std::string> actions_a_;
  std::vector<std::string> topics_b_;
  std::vector<std::string> services_b_;
  std::vector<std::string> actions_b_;
  std::atomic<size_t> current_snapshot_index_{0};
};

inline GraphNode::GraphNode(CommandQueue& incoming, CommandQueue& outgoing,
                            tf::Executor& executor)
    : rclcpp::Node("roscraft_graph_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      query_consumer_(incoming.MakeConsumerToken<QueryGraphCmd>()),
      snapshot_producer_(outgoing.MakeProducerToken<GraphSnapshotCmd>()) {
  using namespace std::chrono_literals;

  // Fallback timer — fires every 500 ms to catch any missed graph events.
  poll_timer_ = create_wall_timer(500ms, [this] { OnPollTimer(); });

  // Get a graph-event handle that will be signalled by the graph listener
  // whenever the topology changes (nodes/topics/services appear or disappear).
  graph_event_ = get_graph_event();

  // Seed the cache so the first query is answered immediately.
  RefreshSnapshot();

  // Start the watcher task last so all members are fully initialised.
  watcher_task_ = executor.async([this] { WatcherTaskFunc(); });
}

inline GraphNode::~GraphNode() {
  stop_watcher_.store(true, std::memory_order_release);
  // Unblock wait_for_graph_change() by signalling the event ourselves.
  if (graph_event_) {
    graph_event_->set();
  }
  if (watcher_task_.valid()) {
    watcher_task_.wait();
  }
}

inline size_t GraphNode::CurrentSnapshotIndex() const noexcept {
  return current_snapshot_index_.load(std::memory_order_acquire);
}

inline size_t GraphNode::PendingSnapshotIndex() const noexcept {
  return 1UL - CurrentSnapshotIndex();
}

}  // namespace roscraft::bridge
