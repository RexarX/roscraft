#include <doctest/doctest.h>

#include <sstream>

#define private public
#include <roscraft/bridge/nodes/ros/action/send_goal.hpp>
#undef private

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/action.hpp>

#include <rclcpp/rclcpp.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <thread>

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

class ScopedSpinExecutor {
public:
  ScopedSpinExecutor() {
    spin_thread_ = std::thread([this] { executor_.spin(); });
  }

  ScopedSpinExecutor(const ScopedSpinExecutor&) = delete;
  ScopedSpinExecutor(ScopedSpinExecutor&&) = delete;
  ~ScopedSpinExecutor() {
    executor_.cancel();
    if (spin_thread_.joinable()) {
      spin_thread_.join();
    }
  }

  ScopedSpinExecutor& operator=(const ScopedSpinExecutor&) = delete;
  ScopedSpinExecutor& operator=(ScopedSpinExecutor&&) = delete;

  void AddNode(const std::shared_ptr<rclcpp::Node>& node) {
    executor_.add_node(node);
  }

private:
  rclcpp::executors::MultiThreadedExecutor executor_;
  std::thread spin_thread_;
};

void RegisterQueues(CommandQueue& incoming, CommandQueue& outgoing) {
  incoming.Register<ActionSendGoalCmd>();
  outgoing.Register<ActionFeedbackCmd>();
  outgoing.Register<ActionResultCmd>();
}

}  // namespace

TEST_SUITE("bridge::ActionSendGoalNode") {
  TEST_CASE("bridge::ActionSendGoalNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    ActionSendGoalNode node(incoming, outgoing,
                            std::pmr::get_default_resource());

    CHECK(std::string_view(node.get_name()) ==
          "roscraft_action_send_goal_node");
    CHECK_NE(node.poll_timer_, nullptr);
    CHECK_NE(node.action_group_, nullptr);
  }

  TEST_CASE("bridge::ActionSendGoalNode::DrainActionSendGoalCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ActionSendGoalNode node(incoming, outgoing,
                            std::pmr::get_default_resource());

    SUBCASE("Dispatches with type introspection error for unknown type") {
      ActionSendGoalCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 401;
      cmd.action_name = "/fibonacci";
      cmd.action_type = "custom_interfaces/action/Unknown";
      cmd.goal_payload.assign({'5'});
      incoming.Enqueue(std::move(cmd));

      node.DrainActionSendGoalCommands();

      ActionResultCmd result(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ActionResultCmd>().Dequeue(result));
      CHECK_EQ(result.request_id, 401);
      CHECK_FALSE(result.success);
      CHECK_NE(result.result_text.find("Failed to load"), std::string::npos);
    }

    SUBCASE("No-ops on empty queue") {
      node.DrainActionSendGoalCommands();
      CHECK_EQ(outgoing.CommandCount<ActionResultCmd>(), 0U);
      CHECK_EQ(outgoing.CommandCount<ActionFeedbackCmd>(), 0U);
    }
  }

  TEST_CASE("bridge::ActionSendGoalNode::DispatchSendGoal") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ActionSendGoalNode node(incoming, outgoing,
                            std::pmr::get_default_resource());

    SUBCASE("Rejects empty action name") {
      ActionSendGoalCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 411;
      cmd.action_name = "";
      cmd.action_type = "example_interfaces/action/Fibonacci";
      cmd.goal_payload.assign({'5'});

      node.DispatchSendGoal(cmd);

      ActionResultCmd result(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ActionResultCmd>().Dequeue(result));
      CHECK_EQ(result.request_id, 411);
      CHECK_FALSE(result.success);
    }

    SUBCASE("Rejects empty action type") {
      ActionSendGoalCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 412;
      cmd.action_name = "/fibonacci";
      cmd.action_type = "";
      cmd.goal_payload.assign({'5'});

      node.DispatchSendGoal(cmd);

      ActionResultCmd result(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ActionResultCmd>().Dequeue(result));
      CHECK_EQ(result.request_id, 412);
      CHECK_FALSE(result.success);
    }

    SUBCASE("Rejects negative timeout") {
      ActionSendGoalCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 413;
      cmd.action_name = "/fibonacci";
      cmd.action_type = "example_interfaces/action/Fibonacci";
      cmd.timeout_seconds = -1.0;
      cmd.goal_payload.assign({'5'});

      node.DispatchSendGoal(cmd);

      ActionResultCmd result(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ActionResultCmd>().Dequeue(result));
      CHECK_EQ(result.request_id, 413);
      CHECK_FALSE(result.success);
      CHECK_EQ(result.result_text, "timeout_seconds must be >= 0.0");
    }

    SUBCASE("Rejects unknown action type with introspection error") {
      ActionSendGoalCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 414;
      cmd.action_name = "/fibonacci";
      cmd.action_type = "custom_interfaces/action/Unknown";
      cmd.goal_payload.assign({'5'});

      node.DispatchSendGoal(cmd);

      ActionResultCmd result(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ActionResultCmd>().Dequeue(result));
      CHECK_EQ(result.request_id, 414);
      CHECK_FALSE(result.success);
    }
  }

  TEST_CASE("bridge::ActionSendGoalNode::OnGoalTimeout") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ActionSendGoalNode node(incoming, outgoing,
                            std::pmr::get_default_resource());

    SUBCASE("Emits timeout result and removes session") {
      ActionSendGoalNode::ActionGoalSession session{};
      session.request_id = 421;
      session.action_name = "/fibonacci";
      session.action_type = "example_interfaces/action/Fibonacci";
      session.feedback = false;
      node.sessions_.emplace(421, std::move(session));

      node.OnGoalTimeout(421);

      ActionResultCmd result(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ActionResultCmd>().Dequeue(result));
      CHECK_EQ(result.request_id, 421);
      CHECK_FALSE(result.success);
      CHECK_EQ(result.result_text, "Action goal timed out");
      CHECK_FALSE(node.sessions_.contains(421));
    }

    SUBCASE("Cancels active goal before emitting timeout") {
      bool cancel_called = false;

      ActionSendGoalNode::ActionGoalSession session{};
      session.request_id = 422;
      session.action_name = "/fibonacci";
      session.action_type = "example_interfaces/action/Fibonacci";
      session.feedback = false;
      session.cancel_goal = [&cancel_called] { cancel_called = true; };
      node.sessions_.emplace(422, std::move(session));

      node.OnGoalTimeout(422);

      ActionResultCmd result(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ActionResultCmd>().Dequeue(result));
      CHECK_EQ(result.request_id, 422);
      CHECK_FALSE(result.success);
      CHECK_EQ(result.result_text, "Action goal timed out");
      CHECK(cancel_called);
      CHECK_FALSE(node.sessions_.contains(422));
    }
  }

  TEST_CASE("bridge::ActionSendGoalNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ActionSendGoalNode node(incoming, outgoing,
                            std::pmr::get_default_resource());

    ActionSendGoalCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 431;
    cmd.action_name = "/fibonacci";
    cmd.action_type = "custom_interfaces/action/Unknown";
    cmd.goal_payload.assign({'4'});
    incoming.Enqueue(std::move(cmd));

    node.OnPollTimer();

    ActionResultCmd result(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<ActionResultCmd>().Dequeue(result));
    CHECK_EQ(result.request_id, 431);
    CHECK_FALSE(result.success);
  }

  TEST_CASE("bridge::ActionSendGoalNode::SendFeedback") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ActionSendGoalNode node(incoming, outgoing,
                            std::pmr::get_default_resource());

    const std::array<uint8_t, 0> empty_payload{};
    node.SendFeedback(441, "/fibonacci", "example_interfaces/action/Fibonacci",
                      std::span<const uint8_t>(empty_payload),
                      "partial_sequence=[1, 1, 2, 3]");

    ActionFeedbackCmd cmd(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<ActionFeedbackCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 441);
    CHECK_EQ(cmd.action_name, "/fibonacci");
    CHECK_EQ(cmd.action_type, "example_interfaces/action/Fibonacci");
    CHECK_EQ(cmd.feedback_text, "partial_sequence=[1, 1, 2, 3]");
    CHECK(cmd.feedback_payload.empty());
  }

  TEST_CASE("bridge::ActionSendGoalNode::SendResult") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ActionSendGoalNode node(incoming, outgoing,
                            std::pmr::get_default_resource());

    const std::array<uint8_t, 3> payload{1U, 2U, 3U};
    node.SendResult(
        451, "/fibonacci", "example_interfaces/action/Fibonacci", true,
        std::span<const uint8_t>(payload.data(), payload.size()), "done");

    ActionResultCmd cmd(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<ActionResultCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 451);
    CHECK(cmd.success);
    CHECK_EQ(cmd.result_text, "done");
    CHECK_EQ(cmd.result_payload.size(), 3U);
  }

  TEST_CASE("bridge::ActionSendGoalNode::RemoveSession") {
    using namespace std::chrono_literals;

    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ActionSendGoalNode node(incoming, outgoing,
                            std::pmr::get_default_resource());

    ActionSendGoalNode::ActionGoalSession session{};
    session.request_id = 461;
    session.action_name = "/fibonacci";
    session.action_type = "example_interfaces/action/Fibonacci";
    session.feedback = false;
    session.timeout_timer = node.create_wall_timer(50ms, [] {});
    node.sessions_.emplace(461, std::move(session));
    CHECK(node.sessions_.contains(461));

    node.RemoveSession(461);

    CHECK_FALSE(node.sessions_.contains(461));
  }

  TEST_CASE("bridge::ActionSendGoalNode caches generic action clients") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ActionSendGoalNode node(incoming, outgoing,
                            std::pmr::get_default_resource());

    SUBCASE("Reuses entry for same action name and type") {
      auto entry1 = node.EnsureGenericClientEntry(
          "/test_action", "example_interfaces/action/Fibonacci");
      REQUIRE(entry1.has_value());

      auto entry2 = node.EnsureGenericClientEntry(
          "/test_action", "example_interfaces/action/Fibonacci");
      REQUIRE(entry2.has_value());

      CHECK_EQ(std::addressof(entry1->get()), std::addressof(entry2->get()));
    }

    SUBCASE("Rejects mismatched type for same action name") {
      auto entry1 = node.EnsureGenericClientEntry(
          "/test_action2", "example_interfaces/action/Fibonacci");
      REQUIRE(entry1.has_value());

      auto entry2 = node.EnsureGenericClientEntry(
          "/test_action2", "turtlesim/action/RotateAbsolute");
      CHECK_FALSE(entry2.has_value());
      CHECK_NE(entry2.error().find("already cached"), std::string::npos);
    }
  }

  TEST_CASE(
      "bridge::ActionSendGoalNode invalidates cached client on unavailable "
      "server") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ActionSendGoalNode node(incoming, outgoing,
                            std::pmr::get_default_resource());

    ActionSendGoalCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 475;
    cmd.action_name = "/definitely_missing_action_server";
    cmd.action_type = "example_interfaces/action/Fibonacci";
    cmd.goal_payload.assign({'5'});
    cmd.timeout_seconds = 0.01;
    cmd.feedback = false;

    node.DispatchSendGoal(cmd);

    ActionResultCmd result(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<ActionResultCmd>().Dequeue(result));
    CHECK_EQ(result.request_id, 475);
    CHECK_FALSE(result.success);
    CHECK_FALSE(node.generic_action_clients_.contains(
        "/definitely_missing_action_server"));
  }
}
