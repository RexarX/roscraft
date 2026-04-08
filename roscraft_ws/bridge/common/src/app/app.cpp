#include <pch.hpp>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/app/config.hpp>
#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/nodes/graph.hpp>
#include <roscraft/bridge/nodes/topic_relay.hpp>

#include <rclcpp/logging.hpp>
#include <rclcpp/rclcpp.hpp>

#include <atomic>
#include <exception>

namespace roscraft::bridge {

void App::Init(AppConfig config) {
  ROSCRAFT_ASSERT(config.Valid(), "Config is invalid!");
  ROSCRAFT_ASSERT(State() == AppState::kUninitialized,
                  "App is already initialized!");

  RegisterAllCommandTypes();

  bridge_ = std::move(config.bridge);
  bridge_->Init(*this);

  InitROS(config.argc, config.argv);

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
                                      std::memory_order_acq_rel)) {
    return;
  }

  // Signal shutdown to all waiting threads
  RequestShutdown();

  // Shutdown ROS first to unblock spin()
  ShutdownROS();

  if (bridge_ != nullptr) {
    bridge_->Destroy(*this);
  }
  bridge_.reset();

  executor_.wait_for_all();

  outgoing_queue_.Clear();
  incoming_queue_.Clear();
  ResetAllocator();

  state_.store(AppState::kUninitialized, std::memory_order_release);
}

void App::InitROS(int argc, char* argv[]) {
  rclcpp::init(argc, argv);

  RegisterAllNodes();

  ros_spin_task_ = executor_.async([this] { SpinROS(); });
}

void App::SpinROS() {
  try {
    ros_executor_.spin();
  } catch (const std::exception& e) {
    RCLCPP_ERROR(rclcpp::get_logger("roscraft_app"),
                 "Exception in ROS spin: %s", e.what());
    RequestShutdown();
  } catch (...) {
    RCLCPP_ERROR(rclcpp::get_logger("roscraft_app"),
                 "Unknown exception in ROS spin");
    RequestShutdown();
  }
}

void App::ShutdownROS() {
  if (rclcpp::ok()) [[likely]] {
    rclcpp::shutdown();
  }

  if (ros_spin_task_.valid()) {
    ros_spin_task_.wait();
  }

  UnregisterAllNodes();
}

void App::UnregisterAllNodes() {
  for (auto& node : nodes_) {
    if (node != nullptr) {
      ros_executor_.remove_node(node);
    }
  }
  nodes_.clear();
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
  outgoing_queue_.Register<PlaceBlockCmd>();
}

void App::RegisterAllNodes() {
  AddNode(std::make_shared<GraphNode>(IncomingQueue(), OutgoingQueue(),
                                      Executor()));
  AddNode(std::make_shared<TopicRelayNode>(IncomingQueue(), OutgoingQueue()));
}

}  // namespace roscraft::bridge
