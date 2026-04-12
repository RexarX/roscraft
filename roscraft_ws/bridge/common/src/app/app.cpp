#include <pch.hpp>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/app/config.hpp>
#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/nodes/graph.hpp>
#include <roscraft/bridge/nodes/topic_relay.hpp>
#include <roscraft/stacktrace.hpp>

#include <rclcpp/logging.hpp>
#include <rclcpp/rclcpp.hpp>

#include <atomic>
#include <exception>

namespace roscraft::bridge {

void App::Init(AppConfig config) {
  ROSCRAFT_ASSERT(config.Valid(), "Config is invalid!");
  ROSCRAFT_ASSERT(State() == AppState::kUninitialized,
                  "App is already initialized!");

  RCLCPP_INFO(rclcpp::get_logger("App"), "Initializing application...");

  RegisterAllCommandTypes();

  bridge_ = std::move(config.bridge);
  bridge_->Init(*this);

  InitROS(config.argc, config.argv);

  RCLCPP_INFO(rclcpp::get_logger("App"), "App initialized successfully");

  state_.store(AppState::kInitialized, std::memory_order_release);
}

void App::Tick() {
  if (State() != AppState::kInitialized) [[unlikely]] {
    return;
  }
  bridge_->Tick(*this);
  AdvanceFrame();
}

void App::Shutdown() {
  AppState expected = AppState::kInitialized;
  if (!state_.compare_exchange_strong(expected, AppState::kShuttingDown,
                                      std::memory_order_acq_rel)) [[unlikely]] {
    return;
  }

  RCLCPP_INFO(rclcpp::get_logger("App"), "Shutting down application...");

  // Signal shutdown to all waiting threads
  RequestShutdown();

  // Clean up ROS resources before destroying the bridge
  CleanUpROS();

  if (bridge_ != nullptr) [[likely]] {
    bridge_->Destroy(*this);
  }
  bridge_.reset();

  executor_.wait_for_all();

  outgoing_queue_.Clear();
  incoming_queue_.Clear();
  ResetAllocator();

  ShutdownROS();

  RCLCPP_INFO(rclcpp::get_logger("App"), "Application shut down successfully");

  state_.store(AppState::kUninitialized, std::memory_order_release);
}

void App::InitROS(int argc, char* argv[]) {
  if (!rclcpp::ok()) [[likely]] {
    rclcpp::init(argc, argv);
  } else {
    RCLCPP_WARN(rclcpp::get_logger("App"),
                "App is being initialized with existing ROS context!");
  }

  ros_executor_.emplace();

  RegisterAllNodes();

  ros_spin_task_ = executor_.async([this] { SpinROS(); });
}

void App::SpinROS() {
  try {
    ros_executor_->spin();
  } catch (const std::exception& e) {
    const auto st = roscraft::Stacktrace::FromCurrentException();
    RCLCPP_ERROR(rclcpp::get_logger("App"), "Exception in ROS spin: %s!\n%s",
                 e.what(), st.ToString().c_str());
    RequestShutdown();
  } catch (...) {
    const auto st = roscraft::Stacktrace::FromCurrentException();

    RCLCPP_ERROR(rclcpp::get_logger("App"),
                 "Unknown exception in ROS spin!\n%s", st.ToString().c_str());
    RequestShutdown();
  }
}

void App::CleanUpROS() {
  if (ros_executor_.has_value()) [[likely]] {
    ros_executor_->cancel();
  }

  if (ros_spin_task_.valid()) [[likely]] {
    ros_spin_task_.wait();
  }

  UnregisterAllNodes();
}

void App::ShutdownROS() {
  if (rclcpp::ok()) [[likely]] {
    rclcpp::shutdown();
  }
}

void App::UnregisterAllNodes() {
  for (auto& node : nodes_) {
    if (node != nullptr) [[likely]] {
      ros_executor_->remove_node(node);
    }
  }
  nodes_.clear();
  ros_executor_.reset();
}

void App::RegisterAllCommandTypes() {
  // Incoming (mod -> ROS)
  incoming_queue_.Register<QueryGraphCmd>();
  incoming_queue_.Register<SubscribeTopicCmd>();
  incoming_queue_.Register<PublishMessageCmd>();
  incoming_queue_.Register<QueryPlayersCmd>();

  // Outgoing (ROS -> mod)
  outgoing_queue_.Register<GraphSnapshotCmd>();
  outgoing_queue_.Register<TopicPayloadCmd>();
  outgoing_queue_.Register<PlayerListCmd>();
}

void App::RegisterAllNodes() {
  AddNode(std::make_shared<GraphNode>(IncomingQueue(), OutgoingQueue(),
                                      Executor()));
  AddNode(std::make_shared<TopicRelayNode>(IncomingQueue(), OutgoingQueue()));
}

}  // namespace roscraft::bridge
