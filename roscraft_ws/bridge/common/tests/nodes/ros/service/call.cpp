#include <doctest/doctest.h>

#include <sstream>

#define private public
#include <roscraft/bridge/nodes/ros/service/call.hpp>
#undef private

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/service.hpp>

#include <rclcpp/rclcpp.hpp>

#include <std_srvs/srv/empty.hpp>
#include <std_srvs/srv/set_bool.hpp>

#include <chrono>
#include <memory>
#include <memory_resource>
#include <span>
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
  incoming.Register<ServiceCallCmd>();
  outgoing.Register<ServiceCallResponseCmd>();
}

[[nodiscard]] auto ToPayload(std::string_view text) -> std::vector<uint8_t> {
  return {text.begin(), text.end()};
}

}  // namespace

TEST_SUITE("bridge::ServiceCallNode") {
  TEST_CASE("bridge::ServiceCallNode::ctor") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);

    ServiceCallNode node(incoming, outgoing, std::pmr::get_default_resource());

    CHECK(std::string_view(node.get_name()) == "roscraft_service_call_node");
    CHECK_NE(node.poll_timer_, nullptr);
    CHECK_NE(node.client_group_, nullptr);
    CHECK_EQ(node.generic_service_clients_.size(), 0U);
  }

  TEST_CASE("bridge::ServiceCallNode::DrainServiceCallCommands") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ServiceCallNode node(incoming, outgoing, std::pmr::get_default_resource());

    ServiceCallCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 11;
    cmd.service_name = "";
    cmd.service_type = "std_srvs/srv/SetBool";

    incoming.Enqueue(std::move(cmd));
    node.DrainServiceCallCommands();

    ServiceCallResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<ServiceCallResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 11U);
    CHECK_FALSE(response.success);
    CHECK_EQ(response.result_text,
             "Service name and service type must be non-empty");
  }

  TEST_CASE("bridge::ServiceCallNode::DispatchServiceCall") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ServiceCallNode node(incoming, outgoing, std::pmr::get_default_resource());

    SUBCASE("Rejects empty service name") {
      ServiceCallCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 21;
      cmd.service_name = "";
      cmd.service_type = "std_srvs/srv/SetBool";

      node.DispatchServiceCall(cmd);

      ServiceCallResponseCmd response(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ServiceCallResponseCmd>().Dequeue(response));
      CHECK_EQ(response.request_id, 21U);
      CHECK_FALSE(response.success);
      CHECK_EQ(response.result_text,
               "Service name and service type must be non-empty");
    }

    SUBCASE("Rejects empty service type") {
      ServiceCallCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 22;
      cmd.service_name = "/service_call_test";
      cmd.service_type = "";

      node.DispatchServiceCall(cmd);

      ServiceCallResponseCmd response(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ServiceCallResponseCmd>().Dequeue(response));
      CHECK_EQ(response.request_id, 22U);
      CHECK_FALSE(response.success);
      CHECK_EQ(response.result_text,
               "Service name and service type must be non-empty");
    }

    SUBCASE("Rejects negative timeout") {
      ServiceCallCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 23;
      cmd.service_name = "/service_call_test";
      cmd.service_type = "std_srvs/srv/SetBool";
      cmd.timeout_seconds = -1.0;

      node.DispatchServiceCall(cmd);

      ServiceCallResponseCmd response(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ServiceCallResponseCmd>().Dequeue(response));
      CHECK_EQ(response.request_id, 23U);
      CHECK_FALSE(response.success);
      CHECK_EQ(response.result_text, "timeout_seconds must be >= 0.0");
    }

    SUBCASE("Rejects negative rate") {
      ServiceCallCmd cmd(std::pmr::get_default_resource());
      cmd.request_id = 24;
      cmd.service_name = "/service_call_test";
      cmd.service_type = "std_srvs/srv/SetBool";
      cmd.rate_hz = -5.0;

      node.DispatchServiceCall(cmd);

      ServiceCallResponseCmd response(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ServiceCallResponseCmd>().Dequeue(response));
      CHECK_EQ(response.request_id, 24U);
      CHECK_FALSE(response.success);
      CHECK_EQ(response.result_text, "rate_hz must be >= 0.0");
    }
  }

  TEST_CASE("bridge::ServiceCallNode::InvokeServiceCall") {
    ScopedRosContext ros_context;

    SUBCASE("Returns false and sends error for unavailable service") {
      CommandQueue incoming;
      CommandQueue outgoing;
      RegisterQueues(incoming, outgoing);
      ServiceCallNode node(incoming, outgoing,
                           std::pmr::get_default_resource());

      const auto payload = ToPayload("data: true\n");
      const bool invoked = node.InvokeServiceCall(
          31, "/missing_service", "std_srvs/srv/SetBool",
          std::span<const uint8_t>(payload.data(), payload.size()), 0.01);

      CHECK_FALSE(invoked);

      ServiceCallResponseCmd response(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ServiceCallResponseCmd>().Dequeue(response));
      CHECK_EQ(response.request_id, 31U);
      CHECK_FALSE(response.success);
      CHECK_FALSE(response.result_text.empty());
    }

    SUBCASE("Returns true and sends response when service succeeds") {
      using namespace std::chrono_literals;

      CommandQueue incoming;
      CommandQueue outgoing;
      RegisterQueues(incoming, outgoing);

      auto node = std::make_shared<ServiceCallNode>(
          incoming, outgoing, std::pmr::get_default_resource());
      auto server_node = std::make_shared<rclcpp::Node>("service_call_server");
      auto service = server_node->create_service<std_srvs::srv::SetBool>(
          "/service_call_invoke_test",
          [](const std::shared_ptr<rmw_request_id_t>,
             const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
             std::shared_ptr<std_srvs::srv::SetBool::Response> response) {
            response->success = request->data;
            response->message = request->data ? "enabled" : "disabled";
          });
      REQUIRE_NE(service, nullptr);

      ScopedSpinExecutor executor;
      executor.AddNode(node);
      executor.AddNode(server_node);
      std::this_thread::sleep_for(150ms);

      const auto payload = ToPayload("data: true\n");
      const bool invoked = node->InvokeServiceCall(
          32, "/service_call_invoke_test", "std_srvs/srv/SetBool",
          std::span<const uint8_t>(payload.data(), payload.size()), 2.0);

      CHECK(invoked);

      ServiceCallResponseCmd response(std::pmr::get_default_resource());
      CHECK(outgoing.TypedStorage<ServiceCallResponseCmd>().Dequeue(response));
      INFO(response.result_text);
      CHECK_EQ(response.request_id, 32U);
      CHECK(response.success);
      CHECK_NE(response.result_text.find("\"success\":true"),
               std::string::npos);
      CHECK_NE(response.result_text.find("\"message\":\"enabled\""),
               std::string::npos);
      CHECK_FALSE(response.response_payload.empty());
    }
  }

  TEST_CASE("bridge::ServiceCallNode::CallGenericService") {
    ScopedRosContext ros_context;

    SUBCASE("Returns false when service is unavailable") {
      CommandQueue incoming;
      CommandQueue outgoing;
      RegisterQueues(incoming, outgoing);
      ServiceCallNode node(incoming, outgoing,
                           std::pmr::get_default_resource());

      std::vector<uint8_t> response_payload;
      const auto payload = ToPayload("{data: true}");

      const auto result = node.CallGenericService(
          "/missing_service", "std_srvs/srv/SetBool",
          std::span<const uint8_t>(payload.data(), payload.size()),
          std::chrono::milliseconds(25), response_payload);

      CHECK_FALSE(result.has_value());
      CHECK(response_payload.empty());
      CHECK_FALSE(result.error().empty());
    }

    SUBCASE("Returns response payload and text on success") {
      using namespace std::chrono_literals;

      CommandQueue incoming;
      CommandQueue outgoing;
      RegisterQueues(incoming, outgoing);

      auto node = std::make_shared<ServiceCallNode>(
          incoming, outgoing, std::pmr::get_default_resource());
      auto server_node =
          std::make_shared<rclcpp::Node>("service_call_generic_server");
      auto service = server_node->create_service<std_srvs::srv::SetBool>(
          "/service_call_generic_test",
          [](const std::shared_ptr<rmw_request_id_t>,
             const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
             std::shared_ptr<std_srvs::srv::SetBool::Response> response) {
            response->success = request->data;
            response->message = request->data ? "enabled" : "disabled";
          });
      REQUIRE_NE(service, nullptr);

      ScopedSpinExecutor executor;
      executor.AddNode(node);
      executor.AddNode(server_node);
      std::this_thread::sleep_for(150ms);

      std::vector<uint8_t> response_payload;
      const auto payload = ToPayload("data: false\n");

      const auto result = node->CallGenericService(
          "/service_call_generic_test", "std_srvs/srv/SetBool",
          std::span<const uint8_t>(payload.data(), payload.size()), 2s,
          response_payload);

      INFO(*result);
      CHECK(result.has_value());
      CHECK_FALSE(response_payload.empty());
    }
  }

  TEST_CASE("bridge::ServiceCallNode::EnsureGenericClient") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ServiceCallNode node(incoming, outgoing, std::pmr::get_default_resource());

    auto first =
        node.EnsureGenericClient("/service_cache_test", "std_srvs/srv/Empty");
    REQUIRE(first.has_value());
    CHECK(node.generic_service_clients_.contains("/service_cache_test"));
    CHECK_EQ(first->get().service_type, "std_srvs/srv/Empty");
    CHECK_NE(first->get().client, nullptr);

    auto second =
        node.EnsureGenericClient("/service_cache_test", "std_srvs/srv/Empty");
    REQUIRE(second.has_value());
    CHECK_EQ(std::addressof(first->get()), std::addressof(second->get()));

    auto mismatch =
        node.EnsureGenericClient("/service_cache_test", "std_srvs/srv/SetBool");
    CHECK_FALSE(mismatch.has_value());
    CHECK_FALSE(mismatch.error().empty());
  }

  TEST_CASE("bridge::ServiceCallNode::SerializeRequestPayload") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ServiceCallNode node(incoming, outgoing, std::pmr::get_default_resource());

    const auto introspection =
        details::LoadServiceIntrospection("std_srvs/srv/SetBool");
    REQUIRE(introspection.has_value());

    const auto payload = ToPayload("data: true\n");
    auto serialized = node.SerializeRequestPayload(
        std::span<const uint8_t>(payload.data(), payload.size()),
        introspection->request);
    if (!serialized.has_value()) {
      INFO(serialized.error().message);
    }
    REQUIRE(serialized.has_value());
    CHECK_FALSE(serialized->empty());

    const auto invalid_payload = ToPayload("{data:");
    auto invalid_serialized = node.SerializeRequestPayload(
        std::span<const uint8_t>(invalid_payload.data(),
                                 invalid_payload.size()),
        introspection->request);
    CHECK_FALSE(invalid_serialized.has_value());

    auto empty_serialized = node.SerializeRequestPayload(
        std::span<const uint8_t>{}, introspection->request);
    REQUIRE(empty_serialized.has_value());
    CHECK(empty_serialized->empty());
  }

  TEST_CASE("bridge::ServiceCallNode::DeserializeResponsePayload") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ServiceCallNode node(incoming, outgoing, std::pmr::get_default_resource());

    const auto introspection =
        details::LoadServiceIntrospection("std_srvs/srv/SetBool");
    REQUIRE(introspection.has_value());

    auto empty_text = node.DeserializeResponsePayload(
        std::span<const uint8_t>{}, introspection->response);
    REQUIRE(empty_text.has_value());
    CHECK_EQ(*empty_text, "{}");

    const auto response_cdr = details::SerializeYamlToCdr(
        "success: true\nmessage: ok\n", introspection->response);
    if (!response_cdr.has_value()) {
      INFO(response_cdr.error().message);
    }
    REQUIRE(response_cdr.has_value());

    auto response_text = node.DeserializeResponsePayload(
        std::span<const uint8_t>(response_cdr->data(), response_cdr->size()),
        introspection->response);
    REQUIRE(response_text.has_value());
    CHECK_NE(response_text->find("\"success\":true"), std::string::npos);
    CHECK_NE(response_text->find("\"message\":\"ok\""), std::string::npos);
  }

  TEST_CASE("bridge::ServiceCallNode::OnPollTimer") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ServiceCallNode node(incoming, outgoing, std::pmr::get_default_resource());

    ServiceCallCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 71;
    cmd.service_name = "";
    cmd.service_type = "std_srvs/srv/SetBool";

    incoming.Enqueue(std::move(cmd));
    node.OnPollTimer();

    ServiceCallResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<ServiceCallResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 71U);
    CHECK_FALSE(response.success);
  }

  TEST_CASE("bridge::ServiceCallNode::ClearRepeatTimer") {
    using namespace std::chrono_literals;

    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ServiceCallNode node(incoming, outgoing, std::pmr::get_default_resource());

    node.repeat_timers_[81] = node.create_wall_timer(10ms, [] {});
    CHECK(node.repeat_timers_.contains(81));

    node.ClearRepeatTimer(81);

    CHECK_FALSE(node.repeat_timers_.contains(81));
  }

  TEST_CASE("bridge::ServiceCallNode::SendResponse") {
    ScopedRosContext ros_context;
    CommandQueue incoming;
    CommandQueue outgoing;
    RegisterQueues(incoming, outgoing);
    ServiceCallNode node(incoming, outgoing, std::pmr::get_default_resource());

    const auto payload = ToPayload("ok");
    node.SendResponse(
        91, "/service_send_response_test", "std_srvs/srv/Empty", true,
        std::span<const uint8_t>(payload.data(), payload.size()), "done");

    ServiceCallResponseCmd response(std::pmr::get_default_resource());
    CHECK(outgoing.TypedStorage<ServiceCallResponseCmd>().Dequeue(response));
    CHECK_EQ(response.request_id, 91U);
    CHECK_EQ(response.service_name, "/service_send_response_test");
    CHECK_EQ(response.service_type, "std_srvs/srv/Empty");
    CHECK(response.success);
    CHECK_EQ(response.result_text, "done");
    CHECK_EQ(response.response_payload.size(), 2U);
    CHECK_EQ(response.response_payload[0], static_cast<uint8_t>('o'));
    CHECK_EQ(response.response_payload[1], static_cast<uint8_t>('k'));
  }
}
