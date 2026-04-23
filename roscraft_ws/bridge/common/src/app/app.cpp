#include <pch.hpp>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/app/config.hpp>
#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/command/commands.hpp>
#include <roscraft/bridge/command/handlers.hpp>
#include <roscraft/bridge/nodes/ros/nodes.hpp>
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

  shutdown_requested_.store(false, std::memory_order_release);

  RCLCPP_INFO(rclcpp::get_logger("App"), "Initializing application...");

  RegisterAllCommandTypes();
  RegisterAllCommandHandlers();

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

  handler_registry_.Clear();

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

void App::RegisterAllNodes() {
  AddNode(std::make_shared<GraphCacheNode>(
      IncomingQueue(), OutgoingQueue(), Executor(), &PendingFrameAllocator()));
  AddNode(std::make_shared<ActionInfoNode>(IncomingQueue(), OutgoingQueue(),
                                           &PendingFrameAllocator()));
  AddNode(std::make_shared<ActionSendGoalNode>(IncomingQueue(), OutgoingQueue(),
                                               &PendingFrameAllocator()));
  AddNode(std::make_shared<InterfaceListNode>(IncomingQueue(), OutgoingQueue(),
                                              &PendingFrameAllocator()));
  AddNode(std::make_shared<InterfaceShowNode>(IncomingQueue(), OutgoingQueue(),
                                              &PendingFrameAllocator()));
  AddNode(std::make_shared<NodeInfoNode>(IncomingQueue(), OutgoingQueue(),
                                         &PendingFrameAllocator()));
  AddNode(std::make_shared<TopicInfoNode>(IncomingQueue(), OutgoingQueue(),
                                          &PendingFrameAllocator()));
  AddNode(std::make_shared<ParamDescribeNode>(IncomingQueue(), OutgoingQueue(),
                                              &PendingFrameAllocator()));
  AddNode(std::make_shared<ParamDumpNode>(IncomingQueue(), OutgoingQueue(),
                                          &PendingFrameAllocator()));
  AddNode(std::make_shared<ParamGetNode>(IncomingQueue(), OutgoingQueue(),
                                         &PendingFrameAllocator()));
  AddNode(std::make_shared<ParamListNode>(IncomingQueue(), OutgoingQueue(),
                                          &PendingFrameAllocator()));
  AddNode(std::make_shared<ParamLoadNode>(IncomingQueue(), OutgoingQueue(),
                                          &PendingFrameAllocator()));
  AddNode(std::make_shared<ParamSetNode>(IncomingQueue(), OutgoingQueue(),
                                         &PendingFrameAllocator()));
  AddNode(std::make_shared<ServiceInfoNode>(IncomingQueue(), OutgoingQueue(),
                                            &PendingFrameAllocator()));
  AddNode(std::make_shared<ServiceCallNode>(IncomingQueue(), OutgoingQueue(),
                                            &PendingFrameAllocator()));
  AddNode(std::make_shared<TopicRelayNode>(IncomingQueue(), OutgoingQueue(),
                                           &PendingFrameAllocator()));
  AddNode(std::make_shared<TopicStatsNode>(IncomingQueue(), OutgoingQueue(),
                                           &PendingFrameAllocator()));
}

void App::UnregisterAllNodes() {
  for (auto& node : nodes_) {
    if (node != nullptr) [[likely]] {
      ros_executor_->remove_node(node);
    }
  }
  nodes_.clear();
  owned_nodes_.clear();
  ros_executor_.reset();
}

void App::RegisterAllCommandTypes() {
  // Incoming (mod -> ROS)
  incoming_queue_.Register<ActionInfoCmd>();
  incoming_queue_.Register<ActionSendGoalCmd>();
  incoming_queue_.Register<QueryGraphCmd>();
  incoming_queue_.Register<InterfaceListCmd>();
  incoming_queue_.Register<InterfaceShowCmd>();
  incoming_queue_.Register<NodeInfoCmd>();
  incoming_queue_.Register<ParamDescribeCmd>();
  incoming_queue_.Register<ParamDumpCmd>();
  incoming_queue_.Register<ParamGetCmd>();
  incoming_queue_.Register<ParamListCmd>();
  incoming_queue_.Register<ParamLoadCmd>();
  incoming_queue_.Register<ParamSetCmd>();
  incoming_queue_.Register<QueryPlayersCmd>();
  incoming_queue_.Register<ServiceCallCmd>();
  incoming_queue_.Register<ServiceInfoCmd>();
  incoming_queue_.Register<TopicInfoCmd>();
  incoming_queue_.Register<TopicSubscribeCmd>();
  incoming_queue_.Register<TopicPublishMessageCmd>();
  incoming_queue_.Register<TopicHzCmd>();
  incoming_queue_.Register<TopicBwCmd>();
  incoming_queue_.Register<TopicDelayCmd>();

  // Outgoing (ROS -> mod)
  outgoing_queue_.Register<ActionInfoResponseCmd>();
  outgoing_queue_.Register<ActionFeedbackCmd>();
  outgoing_queue_.Register<ActionResultCmd>();
  outgoing_queue_.Register<ErrorCmd>();
  outgoing_queue_.Register<GraphSnapshotCmd>();
  outgoing_queue_.Register<InterfaceListResponseCmd>();
  outgoing_queue_.Register<InterfaceShowResponseCmd>();
  outgoing_queue_.Register<NodeInfoResponseCmd>();
  outgoing_queue_.Register<ParamDescribeResponseCmd>();
  outgoing_queue_.Register<ParamDumpResponseCmd>();
  outgoing_queue_.Register<ParamGetResponseCmd>();
  outgoing_queue_.Register<ParamLoadResponseCmd>();
  outgoing_queue_.Register<ParamListResponseCmd>();
  outgoing_queue_.Register<ParamSetResponseCmd>();
  outgoing_queue_.Register<PlayerListCmd>();
  outgoing_queue_.Register<ServiceCallResponseCmd>();
  outgoing_queue_.Register<ServiceInfoResponseCmd>();
  outgoing_queue_.Register<TopicInfoResponseCmd>();
  outgoing_queue_.Register<TopicPayloadCmd>();
  outgoing_queue_.Register<TopicHzResponseCmd>();
  outgoing_queue_.Register<TopicBwResponseCmd>();
  outgoing_queue_.Register<TopicDelayResponseCmd>();
}

void App::RegisterAllCommandHandlers() {
  auto& in = IncomingQueue();
  auto& out = OutgoingQueue();

  handler_registry_.Clear();

  handler_registry_.AddHandler(GraphHandler::From(in, out));
  handler_registry_.AddHandler(NodeInfoHandler::From(in, out));
  handler_registry_.AddHandler(TopicInfoHandler::From(in, out));
  handler_registry_.AddHandler(ServiceInfoHandler::From(in, out));
  handler_registry_.AddHandler(InterfaceListHandler::From(in, out));
  handler_registry_.AddHandler(InterfaceShowHandler::From(in, out));
  handler_registry_.AddHandler(TopicSubscribeHandler::From(in));
  handler_registry_.AddHandler(TopicPublishMessageHandler::From(in));
  handler_registry_.AddHandler(TopicHzHandler::From(in, out));
  handler_registry_.AddHandler(TopicBwHandler::From(in, out));
  handler_registry_.AddHandler(TopicDelayHandler::From(in, out));
  handler_registry_.AddHandler(ServiceCallHandler::From(in, out));
  handler_registry_.AddHandler(ParamListHandler::From(in, out));
  handler_registry_.AddHandler(ParamGetHandler::From(in, out));
  handler_registry_.AddHandler(ParamSetHandler::From(in, out));
  handler_registry_.AddHandler(ParamDescribeHandler::From(in, out));
  handler_registry_.AddHandler(ParamDumpHandler::From(in, out));
  handler_registry_.AddHandler(ParamLoadHandler::From(in, out));
  handler_registry_.AddHandler(ActionInfoHandler::From(in, out));
  handler_registry_.AddHandler(ActionSendGoalHandler::From(in, out));
  handler_registry_.AddHandler(ActionFeedbackHandler::From(out));
  handler_registry_.AddHandler(PlayerListHandler::From(in, out));
  handler_registry_.AddHandler(TopicPayloadHandler::From(out));
  handler_registry_.AddHandler(ErrorHandler::From(out));
}

}  // namespace roscraft::bridge
