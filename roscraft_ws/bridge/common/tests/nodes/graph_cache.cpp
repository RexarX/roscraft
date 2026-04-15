#include <doctest/doctest.h>

#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/queue.hpp>

#include <rclcpp/rclcpp.hpp>

#include <taskflow/taskflow.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <ranges>

#define private public
#include <roscraft/bridge/nodes/graph_cache.hpp>
#undef private

using namespace roscraft::bridge;

namespace {

class ScopedRosContext {
public:
  ScopedRosContext() {
    if (!rclcpp::ok()) {
      int argc = 0;
      rclcpp::init(argc, nullptr);
      owns_context_ = true;
    }
  }

  ScopedRosContext(const ScopedRosContext&) = delete;
  ScopedRosContext(ScopedRosContext&&) = delete;
  ~ScopedRosContext() {
    if (owns_context_ && rclcpp::ok()) {
      rclcpp::shutdown();
    }
  }

  ScopedRosContext& operator=(const ScopedRosContext&) = delete;
  ScopedRosContext& operator=(ScopedRosContext&&) = delete;

private:
  bool owns_context_ = false;
};

void RegisterQueues(CommandQueue& incoming, CommandQueue& outgoing) {
  incoming.Register<QueryGraphCmd>();
  outgoing.Register<GraphSnapshotCmd>();
}

auto DequeueSnapshot(CommandQueue& outgoing) -> GraphSnapshotCmd {
  GraphSnapshotCmd snapshot(std::pmr::get_default_resource());
  const bool dequeued =
      outgoing.TypedStorage<GraphSnapshotCmd>().Dequeue(snapshot);
  CHECK(dequeued);
  return snapshot;
}

}  // namespace

TEST_SUITE("bridge::GraphCacheNode") {
  TEST_CASE("bridge::GraphCacheNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;

    GraphCacheNode node(incoming, outgoing, executor);

    CHECK_EQ(std::string_view(node.get_name()), "roscraft_graph_cache_node");
    CHECK_LT(node.CurrentSnapshotIndex(), 2);
  }

  TEST_CASE("bridge::GraphCacheNode::~GraphCacheNode") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;

    {
      GraphCacheNode node(incoming, outgoing, executor);
      CHECK_LT(node.CurrentSnapshotIndex(), 2);
    }

    CHECK(true);
  }

  TEST_CASE("bridge::GraphCacheNode::RefreshSnapshot") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphCacheNode node(incoming, outgoing, executor);

    node.RefreshSnapshot();

    const auto& snapshot = node.CurrentSnapshot();

    CHECK(std::ranges::is_sorted(
        snapshot.topics,
        [](const auto& a, const auto& b) { return a.name < b.name; }));
    CHECK(std::ranges::is_sorted(
        snapshot.services,
        [](const auto& a, const auto& b) { return a.name < b.name; }));
    CHECK(std::ranges::is_sorted(
        snapshot.actions,
        [](const auto& a, const auto& b) { return a.name < b.name; }));
  }

  TEST_CASE("bridge::GraphCacheNode::DrainAndRespond") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphCacheNode node(incoming, outgoing, executor);

    incoming.Enqueue(QueryGraphCmd{.request_id = 11});
    incoming.Enqueue(QueryGraphCmd{.request_id = 12});

    node.DrainAndRespond();

    const auto first = DequeueSnapshot(outgoing);
    const auto second = DequeueSnapshot(outgoing);

    CHECK_EQ(first.request_id, 11);
    CHECK_EQ(second.request_id, 12);
    CHECK_FALSE(outgoing.HasCommands<GraphSnapshotCmd>());
  }

  TEST_CASE("bridge::GraphCacheNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphCacheNode node(incoming, outgoing, executor);

    incoming.Enqueue(QueryGraphCmd{.request_id = 21});

    node.OnPollTimer();

    const auto snapshot = DequeueSnapshot(outgoing);
    CHECK_EQ(snapshot.request_id, 21);
  }

  TEST_CASE("bridge::GraphCacheNode::OnGraphRefreshPost") {
    using namespace std::chrono_literals;

    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphCacheNode node(incoming, outgoing, executor);

    incoming.Enqueue(QueryGraphCmd{.request_id = 31});

    node.pending_graph_refresh_.store(true, std::memory_order_release);
    node.refresh_post_timer_ = node.create_wall_timer(1ns, [] {});

    node.OnGraphRefreshPost();

    CHECK_FALSE(node.pending_graph_refresh_.load(std::memory_order_acquire));
    CHECK_EQ(node.refresh_post_timer_, nullptr);

    const auto snapshot = DequeueSnapshot(outgoing);
    CHECK_EQ(snapshot.request_id, 31);
  }

  TEST_CASE("bridge::GraphCacheNode::WatcherTaskFunc") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphCacheNode node(incoming, outgoing, executor);

    node.stop_watcher_.store(true, std::memory_order_release);
    node.WatcherTaskFunc();

    CHECK(node.stop_watcher_.load(std::memory_order_acquire));
  }

  TEST_CASE("bridge::GraphCacheNode::PendingSnapshotIndex") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphCacheNode node(incoming, outgoing, executor);

    node.current_snapshot_index_.store(0, std::memory_order_release);
    CHECK_EQ(node.PendingSnapshotIndex(), 1);

    node.current_snapshot_index_.store(1, std::memory_order_release);
    CHECK_EQ(node.PendingSnapshotIndex(), 0);
  }

  TEST_CASE("bridge::GraphCacheNode::CurrentSnapshotIndex") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphCacheNode node(incoming, outgoing, executor);

    node.current_snapshot_index_.store(0, std::memory_order_release);
    CHECK_EQ(node.CurrentSnapshotIndex(), 0);

    node.current_snapshot_index_.store(1, std::memory_order_release);
    CHECK_EQ(node.CurrentSnapshotIndex(), 1);
  }
}
