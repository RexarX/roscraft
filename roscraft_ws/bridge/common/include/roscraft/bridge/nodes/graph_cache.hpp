#pragma once

#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/event.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/timer.hpp>

#include <taskflow/taskflow.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <string>
#include <vector>

namespace roscraft::bridge {

/// @brief Caches ROS graph topology and answers `QueryGraphCmd` requests.
/// @details Maintains a double-buffered snapshot of topics/services/actions and
/// refreshes it both periodically and on graph-change events.
class GraphCacheNode final : public rclcpp::Node {
public:
  /// @brief Construct graph-cache node.
  /// @param incoming Incoming command queue
  /// @param outgoing Outgoing command queue
  /// @param executor Taskflow executor for the graph-watcher task
  GraphCacheNode(CommandQueue& incoming, CommandQueue& outgoing,
                 tf::Executor& executor);

  GraphCacheNode(const GraphCacheNode&) = delete;
  GraphCacheNode(GraphCacheNode&&) = delete;
  ~GraphCacheNode() override;

  GraphCacheNode& operator=(const GraphCacheNode&) = delete;
  GraphCacheNode& operator=(GraphCacheNode&&) = delete;

private:
  struct TopicInfo {
    std::string name;
    std::string type;
  };

  struct ServiceInfo {
    std::string name;
    std::string type;
  };

  struct ActionInfo {
    std::string name;
    std::string type;
  };

  struct SnapshotBuffer {
    std::vector<TopicInfo> topics;
    std::vector<ServiceInfo> services;
    std::vector<ActionInfo> actions;
  };

  /// @brief Refresh graph snapshot from ROS discovery.
  void RefreshSnapshot();

  /// @brief Drain pending queries and respond with cached snapshot.
  void DrainAndRespond();

  /// @brief Timer callback used for periodic refresh/drain.
  void OnPollTimer();

  /// @brief Callback executed on ROS thread after watcher signal.
  void OnGraphRefreshPost();

  /// @brief Graph-watcher async task body.
  void WatcherTaskFunc();

  /// @brief Current active snapshot index.
  [[nodiscard]] size_t CurrentSnapshotIndex() const noexcept {
    return current_snapshot_index_.load(std::memory_order_acquire);
  }

  /// @brief Non-active snapshot index.
  [[nodiscard]] size_t PendingSnapshotIndex() const noexcept {
    return 1UL - CurrentSnapshotIndex();
  }

  /// @brief Current active snapshot buffer.
  [[nodiscard]] SnapshotBuffer& CurrentSnapshot() noexcept {
    return CurrentSnapshotIndex() == 0 ? pending_snapshot_ : current_snapshot_;
  }

  /// @brief Current active snapshot buffer.
  [[nodiscard]] const SnapshotBuffer& CurrentSnapshot() const noexcept {
    return CurrentSnapshotIndex() == 0 ? pending_snapshot_ : current_snapshot_;
  }

  /// @brief Non-active snapshot buffer.
  [[nodiscard]] SnapshotBuffer& PendingSnapshot() noexcept {
    return CurrentSnapshotIndex() == 0 ? current_snapshot_ : pending_snapshot_;
  }

  /// @brief Non-active snapshot buffer.
  [[nodiscard]] const SnapshotBuffer& PendingSnapshot() const noexcept {
    return CurrentSnapshotIndex() == 0 ? current_snapshot_ : pending_snapshot_;
  }

  std::reference_wrapper<CommandQueue> incoming_;
  std::reference_wrapper<CommandQueue> outgoing_;

  CommandQueueConsumerToken query_consumer_;
  CommandQueueProducerToken snapshot_producer_;

  rclcpp::TimerBase::SharedPtr poll_timer_;
  rclcpp::Event::SharedPtr graph_event_;

  std::future<void> watcher_task_;
  std::atomic<bool> stop_watcher_{false};

  std::atomic<bool> pending_graph_refresh_{false};
  rclcpp::TimerBase::SharedPtr refresh_post_timer_;

  SnapshotBuffer current_snapshot_;
  SnapshotBuffer pending_snapshot_;
  std::atomic<size_t> current_snapshot_index_{0U};
};

}  // namespace roscraft::bridge
