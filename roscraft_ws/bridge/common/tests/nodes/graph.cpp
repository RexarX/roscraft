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
#include <roscraft/bridge/nodes/graph.hpp>
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

GraphSnapshotCmd DequeueSnapshot(CommandQueue& outgoing) {
  GraphSnapshotCmd snapshot(std::pmr::get_default_resource());
  const bool dequeued =
      outgoing.TypedStorage<GraphSnapshotCmd>().Dequeue(snapshot);
  CHECK(dequeued);
  return snapshot;
}

}  // namespace

TEST_SUITE("bridge::GraphNode") {
  TEST_CASE("bridge::GraphNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;

    GraphNode node(incoming, outgoing, executor);

    CHECK_EQ(std::string_view(node.get_name()), "roscraft_graph_node");
    CHECK_LT(node.CurrentSnapshotIndex(), 2U);
  }

  TEST_CASE("bridge::GraphNode::~GraphNode") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;

    {
      GraphNode node(incoming, outgoing, executor);
      CHECK_LT(node.CurrentSnapshotIndex(), 2U);
    }

    CHECK(true);
  }

  TEST_CASE("bridge::GraphNode::RefreshSnapshot") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphNode node(incoming, outgoing, executor);

    node.RefreshSnapshot();

    const size_t index = node.CurrentSnapshotIndex();
    const auto& topics = index == 0U ? node.topics_a_ : node.topics_b_;
    const auto& services = index == 0U ? node.services_a_ : node.services_b_;
    const auto& actions = index == 0U ? node.actions_a_ : node.actions_b_;

    CHECK(std::ranges::is_sorted(topics));
    CHECK(std::ranges::is_sorted(services));
    CHECK(std::ranges::is_sorted(actions));
  }

  TEST_CASE("bridge::GraphNode::DrainAndRespond") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphNode node(incoming, outgoing, executor);

    incoming.Enqueue(QueryGraphCmd{.request_id = 11U});
    incoming.Enqueue(QueryGraphCmd{.request_id = 12U});

    node.DrainAndRespond();

    const auto first = DequeueSnapshot(outgoing);
    const auto second = DequeueSnapshot(outgoing);

    CHECK_EQ(first.request_id, 11U);
    CHECK_EQ(second.request_id, 12U);
    CHECK_FALSE(outgoing.HasCommands<GraphSnapshotCmd>());
  }

  TEST_CASE("bridge::GraphNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphNode node(incoming, outgoing, executor);

    incoming.Enqueue(QueryGraphCmd{.request_id = 21U});

    node.OnPollTimer();

    const auto snapshot = DequeueSnapshot(outgoing);
    CHECK_EQ(snapshot.request_id, 21U);
  }

  TEST_CASE("bridge::GraphNode::OnGraphRefreshPost") {
    using namespace std::chrono_literals;

    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphNode node(incoming, outgoing, executor);

    incoming.Enqueue(QueryGraphCmd{.request_id = 31U});

    node.pending_graph_refresh_ = true;
    node.refresh_post_timer_ = node.create_wall_timer(1ns, [] {});

    node.OnGraphRefreshPost();

    CHECK_FALSE(node.pending_graph_refresh_);
    CHECK_EQ(node.refresh_post_timer_, nullptr);

    const auto snapshot = DequeueSnapshot(outgoing);
    CHECK_EQ(snapshot.request_id, 31U);
  }

  TEST_CASE("bridge::GraphNode::WatcherTaskFunc") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphNode node(incoming, outgoing, executor);

    node.stop_watcher_.store(true, std::memory_order_release);
    node.WatcherTaskFunc();

    CHECK(node.stop_watcher_.load(std::memory_order_acquire));
  }

  TEST_CASE("bridge::GraphNode::PendingSnapshotIndex") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphNode node(incoming, outgoing, executor);

    node.current_snapshot_index_.store(0U, std::memory_order_release);
    CHECK_EQ(node.PendingSnapshotIndex(), 1U);

    node.current_snapshot_index_.store(1U, std::memory_order_release);
    CHECK_EQ(node.PendingSnapshotIndex(), 0U);
  }

  TEST_CASE("bridge::GraphNode::CurrentSnapshotIndex") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    tf::Executor executor;
    GraphNode node(incoming, outgoing, executor);

    node.current_snapshot_index_.store(0U, std::memory_order_release);
    CHECK_EQ(node.CurrentSnapshotIndex(), 0U);

    node.current_snapshot_index_.store(1U, std::memory_order_release);
    CHECK_EQ(node.CurrentSnapshotIndex(), 1U);
  }
}
