#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/action/info.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/action.hpp>
#include <roscraft/bridge/nodes/ros/details/graph_utils.hpp>

#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <cstdint>
#include <format>
#include <memory_resource>
#include <string>
#include <string_view>

namespace roscraft::bridge {

namespace {

constexpr std::string_view kSendGoalServiceSuffix = "/_action/send_goal";
constexpr std::string_view kFeedbackTopicSuffix = "/_action/feedback";
constexpr std::string_view kStatusTopicSuffix = "/_action/status";
constexpr std::string_view kFeedbackMessageTypeSuffix = "_FeedbackMessage";
constexpr std::string_view kFeedbackTypeSuffix = "_Feedback";

[[nodiscard]] std::string_view ActionTypeFromFeedbackType(
    std::string_view feedback_type) {
  if (feedback_type.ends_with(kFeedbackMessageTypeSuffix)) {
    const auto type_len =
        feedback_type.size() - kFeedbackMessageTypeSuffix.size();
    return feedback_type.substr(0, type_len);
  }

  if (feedback_type.ends_with(kFeedbackTypeSuffix)) {
    const auto type_len = feedback_type.size() - kFeedbackTypeSuffix.size();
    return feedback_type.substr(0, type_len);
  }

  return {};
}

}  // namespace

ActionInfoNode::ActionInfoNode(CommandQueue& incoming, CommandQueue& outgoing,
                               std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_action_info_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      action_info_consumer_(incoming.MakeConsumerToken<ActionInfoCmd>()),
      action_info_response_producer_(
          outgoing.MakeProducerToken<ActionInfoResponseCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;

  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void ActionInfoNode::DrainActionInfoCommands() {
  auto& in_storage = incoming_.get().TypedStorage<ActionInfoCmd>();
  auto& out_storage = outgoing_.get().TypedStorage<ActionInfoResponseCmd>();

  ActionInfoCmd cmd(allocator_);
  while (in_storage.Dequeue(action_info_consumer_, cmd)) {
    ActionInfoResponseCmd response(allocator_);
    response.request_id = cmd.request_id;

    response.action_name = std::pmr::string(cmd.action_name, allocator_);

    if (cmd.action_name.empty()) {
      out_storage.Enqueue(action_info_response_producer_, std::move(response));
      continue;
    }

    if (!cmd.include_hidden && details::IsHiddenName(cmd.action_name)) {
      out_storage.Enqueue(action_info_response_producer_, std::move(response));
      continue;
    }

    const std::string send_goal_service =
        std::format("{}{}", cmd.action_name, kSendGoalServiceSuffix);
    const std::string feedback_topic =
        std::format("{}{}", cmd.action_name, kFeedbackTopicSuffix);
    const std::string status_topic =
        std::format("{}{}", cmd.action_name, kStatusTopicSuffix);

    response.client_count =
        static_cast<uint32_t>(this->count_clients(send_goal_service));
    response.server_count =
        static_cast<uint32_t>(this->count_services(send_goal_service));
    response.feedback_publisher_count =
        static_cast<uint32_t>(this->count_publishers(feedback_topic));
    response.feedback_subscriber_count =
        static_cast<uint32_t>(this->count_subscribers(feedback_topic));
    response.status_publisher_count =
        static_cast<uint32_t>(this->count_publishers(status_topic));
    response.status_subscriber_count =
        static_cast<uint32_t>(this->count_subscribers(status_topic));

    const auto topic_map = this->get_topic_names_and_types();
    if (const auto it = topic_map.find(feedback_topic); it != topic_map.end()) {
      for (const auto& feedback_type : it->second) {
        const auto action_type = ActionTypeFromFeedbackType(feedback_type);
        if (!action_type.empty()) {
          response.action_type = std::pmr::string(action_type, allocator_);
          break;
        }
      }
      if (response.action_type.empty() && !it->second.empty()) {
        response.action_type = it->second.front();
      }
    }

    out_storage.Enqueue(action_info_response_producer_, std::move(response));
  }
}

void ActionInfoNode::OnPollTimer() {
  DrainActionInfoCommands();
}

}  // namespace roscraft::bridge
