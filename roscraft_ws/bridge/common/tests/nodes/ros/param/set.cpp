#include <doctest/doctest.h>

#include <sstream>

#define private public
#include <roscraft/bridge/nodes/ros/param/set.hpp>
#undef private

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/error.hpp>
#include <roscraft/bridge/command/types/param.hpp>

#include <rclcpp/rclcpp.hpp>

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
  ScopedSpinExecutor()
      : executor_(
            std::make_shared<rclcpp::executors::MultiThreadedExecutor>()) {
    spin_thread_ = std::thread([this] { executor_->spin(); });
  }

  explicit ScopedSpinExecutor(rclcpp::Executor::SharedPtr executor)
      : executor_(std::move(executor)) {
    spin_thread_ = std::thread([this] { executor_->spin(); });
  }

  ScopedSpinExecutor(const ScopedSpinExecutor&) = delete;
  ScopedSpinExecutor(ScopedSpinExecutor&&) = delete;
  ~ScopedSpinExecutor() {
    executor_->cancel();
    if (spin_thread_.joinable()) {
      spin_thread_.join();
    }
  }

  ScopedSpinExecutor& operator=(const ScopedSpinExecutor&) = delete;
  ScopedSpinExecutor& operator=(ScopedSpinExecutor&&) = delete;

  void AddNode(const std::shared_ptr<rclcpp::Node>& node) {
    executor_->add_node(node);
  }

  auto GetExecutor() const -> rclcpp::Executor::SharedPtr { return executor_; }

private:
  rclcpp::Executor::SharedPtr executor_;
  std::thread spin_thread_;
};

void RegisterQueues(CommandQueue& incoming, CommandQueue& outgoing) {
  incoming.Register<ParamSetCmd>();
  outgoing.Register<ParamSetResponseCmd>();
  outgoing.Register<ErrorCmd>();
}

auto MakeParamTargetNode() -> std::shared_ptr<rclcpp::Node> {
  auto node = std::make_shared<rclcpp::Node>("param_target_set_node");
  node->declare_parameter("integer_value", 10);
  node->declare_parameter("string_value", std::string("hello"));

  rcl_interfaces::msg::ParameterDescriptor descriptor;
  descriptor.description = "described parameter";
  descriptor.read_only = false;
  node->declare_parameter("described_value", 1.5, descriptor);

  return node;
}

}  // namespace

TEST_SUITE("bridge::ParamSetNode") {
  TEST_CASE("bridge::ParamSetNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    ParamSetNode node(incoming, outgoing, std::pmr::get_default_resource());

    CHECK(std::string_view(node.get_name()) == "roscraft_param_set_node");
    CHECK_NE(node.poll_timer_, nullptr);
  }

  TEST_CASE("bridge::ParamSetNode::DrainParamSetCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    auto node = std::make_shared<ParamSetNode>(
        incoming, outgoing, std::pmr::get_default_resource());

    SUBCASE("Invalid input") {
      ParamSetCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 31;
      cmd.node_name = "";
      cmd.param_name = "integer_value";
      cmd.value_text = "42";
      incoming.Enqueue(std::move(cmd));

      node->DrainParamSetCommands();

      ErrorCmd err(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(err));
      CHECK_EQ(err.request_id, 31);
      CHECK_EQ(err.error_code, "PARAM_SET_FAILED");
      CHECK_FALSE(err.error_message.empty());
    }

    SUBCASE("Valid input") {
      using namespace std::chrono_literals;

      auto target_node = MakeParamTargetNode();
      auto spin_executor =
          std::make_shared<rclcpp::executors::MultiThreadedExecutor>();
      ScopedSpinExecutor background_spinner(spin_executor);
      background_spinner.AddNode(target_node);
      std::this_thread::sleep_for(100ms);

      ParamSetCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 32;
      cmd.node_name = "/param_target_set_node";
      cmd.param_name = "integer_value";
      cmd.value_text = "42";
      cmd.timeout_seconds = 2.0;
      incoming.Enqueue(std::move(cmd));

      node->DrainParamSetCommands();

      ParamSetResponseCmd response(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(response));
      CHECK_EQ(response.request_id, 32);
      CHECK_EQ(response.node_name, "/param_target_set_node");
      CHECK_EQ(response.param_name, "integer_value");
      CHECK(response.success);
      CHECK_FALSE(response.param_type.empty());
      CHECK_FALSE(response.value_text.empty());
      CHECK_EQ(target_node->get_parameter("integer_value").as_int(), 42);
      CHECK_FALSE(outgoing.HasCommands<ErrorCmd>());
    }
  }

  TEST_CASE("bridge::ParamSetNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ParamSetNode node(incoming, outgoing, std::pmr::get_default_resource());

    ParamSetCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 61;
    cmd.node_name = "";
    incoming.Enqueue(std::move(cmd));

    node.OnPollTimer();

    ErrorCmd err(std::pmr::get_default_resource());
    CHECK(outgoing.Dequeue(err));
    CHECK_EQ(err.request_id, 61);
    CHECK_EQ(err.error_code, "PARAM_SET_FAILED");
  }

  TEST_CASE("bridge::ParamSetNode::SendError") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ParamSetNode node(incoming, outgoing, std::pmr::get_default_resource());

    node.SendError(99, "PARAM_TEST_ERROR", "synthetic error");

    ErrorCmd err(std::pmr::get_default_resource());
    CHECK(outgoing.Dequeue(err));
    CHECK_EQ(err.request_id, 99);
    CHECK_EQ(err.error_code, "PARAM_TEST_ERROR");
    CHECK_EQ(err.error_message, "synthetic error");
  }
}
