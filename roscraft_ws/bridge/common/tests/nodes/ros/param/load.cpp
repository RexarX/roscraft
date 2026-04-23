#include <doctest/doctest.h>

#include <sstream>

#define private public
#include <roscraft/bridge/nodes/ros/param/load.hpp>
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
#include <vector>

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
  incoming.Register<ParamLoadCmd>();
  outgoing.Register<ParamLoadResponseCmd>();
  outgoing.Register<ErrorCmd>();
}

auto MakeParamTargetNode() -> std::shared_ptr<rclcpp::Node> {
  auto node = std::make_shared<rclcpp::Node>("param_target_load_node");
  node->declare_parameter("integer_value", 10);
  node->declare_parameter("string_value", std::string("hello"));
  node->declare_parameter("nested.scalar", 1.0);
  node->declare_parameter("bool_array", std::vector<bool>{false});
  node->declare_parameter("int_array", std::vector<int64_t>{1});
  node->declare_parameter("double_array", std::vector<double>{1.0});
  node->declare_parameter("string_array", std::vector<std::string>{"hello"});
  return node;
}

}  // namespace

TEST_SUITE("bridge::ParamLoadNode") {
  TEST_CASE("bridge::ParamLoadNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    ParamLoadNode node(incoming, outgoing, std::pmr::get_default_resource());

    CHECK(std::string_view(node.get_name()) == "roscraft_param_load_node");
    CHECK_NE(node.poll_timer_, nullptr);
  }

  TEST_CASE("bridge::ParamLoadNode::DrainParamLoadCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ParamLoadNode node(incoming, outgoing, std::pmr::get_default_resource());

    SUBCASE("Invalid input") {
      ParamLoadCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 81;
      cmd.node_name = "";
      cmd.yaml_text = "integer_value: 42";
      incoming.Enqueue(std::move(cmd));

      node.DrainParamLoadCommands();

      ErrorCmd err(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(err));
      CHECK_EQ(err.request_id, 81);
      CHECK_EQ(err.error_code, "PARAM_LOAD_FAILED");
      CHECK_FALSE(err.error_message.empty());
    }

    SUBCASE("Valid input") {
      using namespace std::chrono_literals;

      auto target_node = MakeParamTargetNode();
      ScopedSpinExecutor executor;
      executor.AddNode(target_node);
      std::this_thread::sleep_for(100ms);

      ParamLoadCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 82;
      cmd.node_name = "/param_target_load_node";
      cmd.yaml_text = "integer_value: 42\nstring_value: world\n";
      cmd.timeout_seconds = 2.0;
      incoming.Enqueue(std::move(cmd));

      node.DrainParamLoadCommands();

      ParamLoadResponseCmd response(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(response));
      CHECK_EQ(response.request_id, 82);
      CHECK_EQ(response.node_name, "/param_target_load_node");
      CHECK(response.success);
      CHECK_EQ(response.params_loaded, 2U);
      CHECK_FALSE(outgoing.HasCommands<ErrorCmd>());
      CHECK_EQ(target_node->get_parameter("integer_value").as_int(), 42);
      CHECK_EQ(target_node->get_parameter("string_value").as_string(), "world");
    }

    SUBCASE("Caches parameter client across repeated requests") {
      using namespace std::chrono_literals;

      auto target_node = MakeParamTargetNode();
      ScopedSpinExecutor executor;
      executor.AddNode(target_node);
      std::this_thread::sleep_for(100ms);

      ParamLoadCmd first(std::pmr::get_default_resource());
      first.request_id = 92;
      first.node_name = "/param_target_load_node";
      first.yaml_text = "integer_value: 52\n";
      first.timeout_seconds = 2.0;
      incoming.Enqueue(std::move(first));

      node.DrainParamLoadCommands();

      ParamLoadResponseCmd first_response(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(first_response));
      CHECK_EQ(first_response.request_id, 92);
      CHECK(first_response.success);

      REQUIRE(node.parameter_clients_.contains("/param_target_load_node"));
      const auto* cached_client =
          node.parameter_clients_.at("/param_target_load_node").get();

      ParamLoadCmd second(std::pmr::get_default_resource());
      second.request_id = 93;
      second.node_name = "/param_target_load_node";
      second.yaml_text = "integer_value: 53\n";
      second.timeout_seconds = 2.0;
      incoming.Enqueue(std::move(second));

      node.DrainParamLoadCommands();

      ParamLoadResponseCmd second_response(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(second_response));
      CHECK_EQ(second_response.request_id, 93);
      CHECK(second_response.success);
      CHECK_EQ(target_node->get_parameter("integer_value").as_int(), 53);

      REQUIRE(node.parameter_clients_.contains("/param_target_load_node"));
      CHECK_EQ(node.parameter_clients_.at("/param_target_load_node").get(),
               cached_client);
      CHECK_FALSE(outgoing.HasCommands<ErrorCmd>());
    }

    SUBCASE("Invalidates cached client when service is unavailable") {
      auto stale_client = std::make_shared<rclcpp::SyncParametersClient>(
          &node, "/missing_param_target");
      node.parameter_clients_.emplace("/missing_param_target",
                                      std::move(stale_client));

      ParamLoadCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 94;
      cmd.node_name = "/missing_param_target";
      cmd.yaml_text = "integer_value: 42\n";
      cmd.timeout_seconds = 0.01;
      incoming.Enqueue(std::move(cmd));

      node.DrainParamLoadCommands();

      ErrorCmd err(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(err));
      CHECK_EQ(err.request_id, 94);
      CHECK_EQ(err.error_code, "PARAM_LOAD_FAILED");
      CHECK_EQ(err.error_message,
               "Parameter services unavailable before timeout");
      CHECK_FALSE(node.parameter_clients_.contains("/missing_param_target"));
    }

    SUBCASE("Valid ros2 node-scoped YAML") {
      using namespace std::chrono_literals;

      auto target_node = MakeParamTargetNode();
      ScopedSpinExecutor executor;
      executor.AddNode(target_node);
      std::this_thread::sleep_for(100ms);

      ParamLoadCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 83;
      cmd.node_name = "/param_target_load_node";
      cmd.yaml_text =
          "/param_target_load_node:\n"
          "  ros__parameters:\n"
          "    integer_value: 45\n"
          "    nested:\n"
          "      scalar: 3.5\n"
          "    bool_array: [true, false, true]\n"
          "    int_array: [10, 20, 30]\n"
          "    double_array: [1.25, 2.5]\n"
          "    string_array: [alpha, beta]\n";
      cmd.timeout_seconds = 2.0;
      incoming.Enqueue(std::move(cmd));

      node.DrainParamLoadCommands();

      ParamLoadResponseCmd response(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(response));
      CHECK_EQ(response.request_id, 83);
      CHECK_EQ(response.node_name, "/param_target_load_node");
      CHECK(response.success);
      CHECK_EQ(response.params_loaded, 6U);
      CHECK_FALSE(outgoing.HasCommands<ErrorCmd>());

      CHECK_EQ(target_node->get_parameter("integer_value").as_int(), 45);
      CHECK_EQ(target_node->get_parameter("nested.scalar").as_double(),
               doctest::Approx(3.5));
      CHECK_EQ(target_node->get_parameter("bool_array").as_bool_array(),
               std::vector<bool>{true, false, true});
      CHECK_EQ(target_node->get_parameter("int_array").as_integer_array(),
               std::vector<int64_t>{10, 20, 30});
      CHECK_EQ(target_node->get_parameter("double_array").as_double_array(),
               std::vector<double>{1.25, 2.5});
      CHECK_EQ(target_node->get_parameter("string_array").as_string_array(),
               std::vector<std::string>{"alpha", "beta"});
    }

    SUBCASE("Valid wildcard node-scoped YAML") {
      using namespace std::chrono_literals;

      auto target_node = MakeParamTargetNode();
      ScopedSpinExecutor executor;
      executor.AddNode(target_node);
      std::this_thread::sleep_for(100ms);

      ParamLoadCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 84;
      cmd.node_name = "/param_target_load_node";
      cmd.yaml_text =
          "/**:\n"
          "  ros__parameters:\n"
          "    integer_value: 46\n";
      cmd.timeout_seconds = 2.0;
      incoming.Enqueue(std::move(cmd));

      node.DrainParamLoadCommands();

      ParamLoadResponseCmd response(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(response));
      CHECK_EQ(response.request_id, 84);
      CHECK_EQ(response.node_name, "/param_target_load_node");
      CHECK(response.success);
      CHECK_EQ(response.params_loaded, 1U);
      CHECK_FALSE(outgoing.HasCommands<ErrorCmd>());
      CHECK_EQ(target_node->get_parameter("integer_value").as_int(), 46);
    }

    SUBCASE("Wildcard section ignored when use_wildcard is false") {
      using namespace std::chrono_literals;

      auto target_node = MakeParamTargetNode();
      ScopedSpinExecutor executor;
      executor.AddNode(target_node);
      std::this_thread::sleep_for(100ms);

      ParamLoadCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 89;
      cmd.node_name = "/param_target_load_node";
      cmd.yaml_text =
          "/**:\n"
          "  ros__parameters:\n"
          "    integer_value: 123\n";
      cmd.timeout_seconds = 2.0;
      cmd.use_wildcard = false;
      incoming.Enqueue(std::move(cmd));

      node.DrainParamLoadCommands();

      ParamLoadResponseCmd response(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(response));
      CHECK_EQ(response.request_id, 89);
      CHECK_EQ(response.node_name, "/param_target_load_node");
      CHECK(response.success);
      CHECK_EQ(response.params_loaded, 0U);
      CHECK_EQ(response.reason, "No parameters found in input");
      CHECK_FALSE(outgoing.HasCommands<ErrorCmd>());
      CHECK_EQ(target_node->get_parameter("integer_value").as_int(), 10);
    }

    SUBCASE("Valid wildcard and node-scoped YAML merges with node override") {
      using namespace std::chrono_literals;

      auto target_node = MakeParamTargetNode();
      ScopedSpinExecutor executor;
      executor.AddNode(target_node);
      std::this_thread::sleep_for(100ms);

      ParamLoadCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 87;
      cmd.node_name = "/param_target_load_node";
      cmd.yaml_text =
          "/**:\n"
          "  ros__parameters:\n"
          "    integer_value: 50\n"
          "    string_value: wildcard\n"
          "/param_target_load_node:\n"
          "  ros__parameters:\n"
          "    integer_value: 51\n";
      cmd.timeout_seconds = 2.0;
      incoming.Enqueue(std::move(cmd));

      node.DrainParamLoadCommands();

      ParamLoadResponseCmd response(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(response));
      CHECK_EQ(response.request_id, 87);
      CHECK_EQ(response.node_name, "/param_target_load_node");
      CHECK(response.success);
      CHECK_EQ(response.params_loaded, 2U);
      CHECK_FALSE(outgoing.HasCommands<ErrorCmd>());
      CHECK_EQ(target_node->get_parameter("integer_value").as_int(), 51);
      CHECK_EQ(target_node->get_parameter("string_value").as_string(),
               "wildcard");
    }

    SUBCASE("Invalid node-scoped YAML for other node") {
      using namespace std::chrono_literals;

      auto target_node = MakeParamTargetNode();
      ScopedSpinExecutor executor;
      executor.AddNode(target_node);
      std::this_thread::sleep_for(100ms);

      ParamLoadCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 85;
      cmd.node_name = "/param_target_load_node";
      cmd.yaml_text =
          "/some_other_node:\n"
          "  ros__parameters:\n"
          "    integer_value: 47\n";
      cmd.timeout_seconds = 2.0;
      incoming.Enqueue(std::move(cmd));

      node.DrainParamLoadCommands();

      ErrorCmd err(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(err));
      CHECK_EQ(err.request_id, 85);
      CHECK_EQ(err.error_code, "PARAM_LOAD_FAILED");
      CHECK_NE(err.error_message.find("No YAML section found for node"),
               std::string::npos);
    }

    SUBCASE("Invalid wildcard ros__parameters mapping") {
      using namespace std::chrono_literals;

      auto target_node = MakeParamTargetNode();
      ScopedSpinExecutor executor;
      executor.AddNode(target_node);
      std::this_thread::sleep_for(100ms);

      ParamLoadCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 88;
      cmd.node_name = "/param_target_load_node";
      cmd.yaml_text =
          "/**:\n"
          "  ros__parameters: 42\n";
      cmd.timeout_seconds = 2.0;
      incoming.Enqueue(std::move(cmd));

      node.DrainParamLoadCommands();

      ErrorCmd err(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(err));
      CHECK_EQ(err.request_id, 88);
      CHECK_EQ(err.error_code, "PARAM_LOAD_FAILED");
      CHECK_NE(err.error_message.find("must be a mapping"), std::string::npos);
    }

    SUBCASE("Invalid YAML with unsupported array value") {
      using namespace std::chrono_literals;

      auto target_node = MakeParamTargetNode();
      ScopedSpinExecutor executor;
      executor.AddNode(target_node);
      std::this_thread::sleep_for(100ms);

      ParamLoadCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 86;
      cmd.node_name = "/param_target_load_node";
      cmd.yaml_text = "int_array: [1, two]\n";
      cmd.timeout_seconds = 2.0;
      incoming.Enqueue(std::move(cmd));

      node.DrainParamLoadCommands();

      ErrorCmd err(std::pmr::get_default_resource());
      CHECK(outgoing.Dequeue(err));
      CHECK_EQ(err.request_id, 86);
      CHECK_EQ(err.error_code, "PARAM_LOAD_FAILED");
      CHECK_NE(err.error_message.find("mixed element types"),
               std::string::npos);
    }
  }

  TEST_CASE("bridge::ParamLoadNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ParamLoadNode node(incoming, outgoing, std::pmr::get_default_resource());

    ParamLoadCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 91;
    cmd.node_name = "";
    cmd.yaml_text = "integer_value: 42";
    incoming.Enqueue(std::move(cmd));

    node.OnPollTimer();

    ErrorCmd err(std::pmr::get_default_resource());
    CHECK(outgoing.Dequeue(err));
    CHECK_EQ(err.request_id, 91);
    CHECK_EQ(err.error_code, "PARAM_LOAD_FAILED");
  }

  TEST_CASE("bridge::ParamLoadNode::SendError") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ParamLoadNode node(incoming, outgoing, std::pmr::get_default_resource());

    node.SendError(99, "PARAM_TEST_ERROR", "synthetic error");

    ErrorCmd err(std::pmr::get_default_resource());
    CHECK(outgoing.Dequeue(err));
    CHECK_EQ(err.request_id, 99);
    CHECK_EQ(err.error_code, "PARAM_TEST_ERROR");
    CHECK_EQ(err.error_message, "synthetic error");
  }
}
