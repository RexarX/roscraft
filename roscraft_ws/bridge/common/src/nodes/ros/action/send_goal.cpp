#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/action/send_goal.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/action.hpp>
#include <roscraft/bridge/nodes/ros/common.hpp>
#include <roscraft/bridge/nodes/ros/details/introspection_codec.hpp>
#include <roscraft/memory/arena_allocator.hpp>
#include <roscraft/utils/random.hpp>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp/serialized_message.hpp>

#include <rosidl_runtime_c/message_type_support_struct.h>

#include <rosidl_typesupport_introspection_cpp/field_types.hpp>
#include <rosidl_typesupport_introspection_cpp/message_introspection.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <memory>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rticpp = rosidl_typesupport_introspection_cpp;

namespace roscraft::bridge {

namespace {

struct GoalMemberInfo {
  const rticpp::MessageMembers* goal_members = nullptr;
  size_t goal_offset = 0;
};

struct FeedbackMemberInfo {
  const rticpp::MessageMembers* feedback_members = nullptr;
  size_t feedback_offset = 0;
};

struct ResultMemberInfo {
  const rticpp::MessageMembers* result_members = nullptr;
  size_t result_offset = 0;
  size_t status_offset = 0;
};

using IntrospectionMessageMembers = rticpp::MessageMembers;

[[nodiscard]] auto GenerateGoalUuid() -> std::array<uint8_t, 16> {
  std::array<uint8_t, 16> uuid = {};
  for (auto& byte : uuid) {
    byte = utils::RandomFastValueFromRange(uint8_t{0}, uint8_t{255});
  }
  uuid[6] = (uuid[6] & 0x0F) | 0x40;
  uuid[8] = (uuid[8] & 0x3F) | 0x80;
  return uuid;
}

[[nodiscard]] std::string DeriveTypeSuffix(std::string_view action_type,
                                           std::string_view suffix) {
  std::string result(action_type);
  result.push_back('_');
  result.append(suffix);
  return result;
}

[[nodiscard]] std::string DeriveSendGoalServiceType(
    std::string_view action_type) {
  return DeriveTypeSuffix(action_type, "SendGoal");
}

[[nodiscard]] std::string DeriveGetResultServiceType(
    std::string_view action_type) {
  return DeriveTypeSuffix(action_type, "GetResult");
}

[[nodiscard]] std::string DeriveFeedbackMessageType(
    std::string_view action_type) {
  return DeriveTypeSuffix(action_type, "FeedbackMessage");
}

[[nodiscard]] std::string DeriveGoalMessageType(std::string_view action_type) {
  return DeriveTypeSuffix(action_type, "Goal");
}

[[nodiscard]] auto FindGoalIdOffset(
    const details::MessageIntrospection& request_introspection)
    -> std::optional<size_t> {
  if (request_introspection.message_members == nullptr) [[unlikely]] {
    return std::nullopt;
  }
  return details::FindMemberOffset(*request_introspection.message_members,
                                   "goal_id");
}

[[nodiscard]] auto FindGoalMember(
    const details::MessageIntrospection& request_introspection)
    -> std::optional<GoalMemberInfo> {
  if (request_introspection.message_members == nullptr) [[unlikely]] {
    return std::nullopt;
  }

  for (uint32_t index = 0;
       index < request_introspection.message_members->member_count_; ++index) {
    const auto& member = request_introspection.message_members->members_[index];
    if (member.name_ == nullptr) {
      continue;
    }
    if (std::string_view(member.name_) == "goal") {
      if (member.type_id_ != rticpp::ROS_TYPE_MESSAGE) [[unlikely]] {
        return std::nullopt;
      }
      const auto* goal_members =
          details::ToIntrospectionMembers(member.members_);
      if (goal_members == nullptr) [[unlikely]] {
        return std::nullopt;
      }
      GoalMemberInfo info{};
      info.goal_members = goal_members;
      info.goal_offset = member.offset_;
      return info;
    }
  }

  return std::nullopt;
}

[[nodiscard]] auto FindFeedbackGoalIdOffset(
    const details::MessageIntrospection& feedback_introspection)
    -> std::optional<size_t> {
  return FindGoalIdOffset(feedback_introspection);
}

[[nodiscard]] auto FindFeedbackMember(
    const details::MessageIntrospection& feedback_introspection)
    -> std::optional<FeedbackMemberInfo> {
  if (feedback_introspection.message_members == nullptr) [[unlikely]] {
    return std::nullopt;
  }

  for (uint32_t index = 0;
       index < feedback_introspection.message_members->member_count_; ++index) {
    const auto& member =
        feedback_introspection.message_members->members_[index];
    if (member.name_ == nullptr) {
      continue;
    }
    if (std::string_view(member.name_) == "feedback") {
      if (member.type_id_ != rticpp::ROS_TYPE_MESSAGE) [[unlikely]] {
        return std::nullopt;
      }
      const auto* feedback_members =
          details::ToIntrospectionMembers(member.members_);
      if (feedback_members == nullptr) [[unlikely]] {
        return std::nullopt;
      }
      FeedbackMemberInfo info{};
      info.feedback_members = feedback_members;
      info.feedback_offset = member.offset_;
      return info;
    }
  }

  return std::nullopt;
}

[[nodiscard]] auto FindResultMember(
    const details::ServiceIntrospection& get_result_introspection)
    -> std::optional<ResultMemberInfo> {
  if (get_result_introspection.response.message_members == nullptr)
      [[unlikely]] {
    return std::nullopt;
  }

  ResultMemberInfo info{};
  info.result_members = nullptr;
  info.result_offset = 0;
  info.status_offset = 0;

  for (uint32_t index = 0;
       index < get_result_introspection.response.message_members->member_count_;
       ++index) {
    const auto& member =
        get_result_introspection.response.message_members->members_[index];
    if (member.name_ == nullptr) {
      continue;
    }
    if (std::string_view(member.name_) == "status") {
      info.status_offset = member.offset_;
    }
    if (std::string_view(member.name_) == "result") {
      if (member.type_id_ == rticpp::ROS_TYPE_MESSAGE) {
        info.result_members = details::ToIntrospectionMembers(member.members_);
      }
      info.result_offset = member.offset_;
    }
  }

  if (info.status_offset == 0 && info.result_offset == 0) [[unlikely]] {
    return std::nullopt;
  }

  return info;
}

[[nodiscard]] constexpr std::string_view ResultCodeToText(
    int8_t status_code) noexcept {
  switch (status_code) {
    case 4:
      return "succeeded";
    case 5:
      return "canceled";
    case 6:
      return "aborted";
    default:
      return "unknown";
  }
}

}  // namespace

ActionSendGoalNode::ActionSendGoalNode(CommandQueue& incoming,
                                       CommandQueue& outgoing,
                                       std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_action_send_goal_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      action_send_goal_consumer_(
          incoming.MakeConsumerToken<ActionSendGoalCmd>()),
      action_feedback_producer_(
          outgoing.MakeProducerToken<ActionFeedbackCmd>()),
      action_result_producer_(outgoing.MakeProducerToken<ActionResultCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;

  action_group_ =
      this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

void ActionSendGoalNode::DrainActionSendGoalCommands() {
  auto& storage = incoming_.get().TypedStorage<ActionSendGoalCmd>();

  ActionSendGoalCmd cmd(allocator_);
  while (storage.Dequeue(action_send_goal_consumer_, cmd)) {
    DispatchSendGoal(cmd);
  }
}

void ActionSendGoalNode::DispatchSendGoal(const ActionSendGoalCmd& cmd) {
  if (cmd.action_name.empty() || cmd.action_type.empty()) [[unlikely]] {
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               "Action name and action type must be non-empty");
    return;
  }

  if (cmd.timeout_seconds < 0.0) [[unlikely]] {
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               "timeout_seconds must be >= 0.0");
    return;
  }

  RemoveSession(cmd.request_id);

  auto result = EnsureGenericClientEntry(cmd.action_name, cmd.action_type);
  if (!result.has_value()) [[unlikely]] {
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               result.error());
    return;
  }
  auto& entry = result.value();

  const auto payload = std::span<const uint8_t>(cmd.goal_payload.data(),
                                                cmd.goal_payload.size());
  DispatchGenericGoal(cmd, payload, entry);
}

auto ActionSendGoalNode::EnsureGenericClientEntry(std::string_view action_name,
                                                  std::string_view action_type)
    -> std::expected<std::reference_wrapper<GenericActionClientEntry>,
                     std::string> {
  const auto existing_it = generic_action_clients_.find(action_name);
  if (existing_it != generic_action_clients_.end()) {
    if (existing_it->second.action_type != action_type) [[unlikely]] {
      return std::unexpected(std::format(
          "Action '{}' already cached with type '{}', requested '{}'",
          action_name, existing_it->second.action_type, action_type));
    }
    return std::ref(existing_it->second);
  }

  GenericActionClientEntry new_entry{};
  new_entry.action_type = std::string(action_type);

  const auto send_goal_type = DeriveSendGoalServiceType(action_type);
  const auto get_result_type = DeriveGetResultServiceType(action_type);
  const auto feedback_msg_type = DeriveFeedbackMessageType(action_type);

  auto send_goal_intro = details::LoadServiceIntrospection(send_goal_type);
  if (!send_goal_intro) [[unlikely]] {
    return std::unexpected(send_goal_intro.error().message);
  }
  new_entry.send_goal_introspection = std::move(*send_goal_intro);

  auto get_result_intro = details::LoadServiceIntrospection(get_result_type);
  if (!get_result_intro) [[unlikely]] {
    return std::unexpected(get_result_intro.error().message);
  }
  new_entry.get_result_introspection = std::move(*get_result_intro);

  const std::string kCancelGoalServiceType = "action_msgs/srv/CancelGoal";
  auto cancel_goal_intro =
      details::LoadServiceIntrospection(kCancelGoalServiceType);
  if (!cancel_goal_intro) [[unlikely]] {
    return std::unexpected(cancel_goal_intro.error().message);
  }
  new_entry.cancel_goal_introspection = std::move(*cancel_goal_intro);

  auto feedback_intro = details::LoadMessageIntrospection(feedback_msg_type);
  if (!feedback_intro) [[unlikely]] {
    return std::unexpected(feedback_intro.error().message);
  }
  new_entry.feedback_message_introspection = std::move(*feedback_intro);

  try {
    const auto send_goal_service_name =
        std::format("{}/_action/send_goal", action_name);
    new_entry.send_goal_client =
        this->create_generic_client(send_goal_service_name, send_goal_type,
                                    rclcpp::ServicesQoS(), action_group_);
  } catch (const std::exception& ex) {
    return std::unexpected(
        std::format("Failed to create send_goal client: {}", ex.what()));
  } catch (...) {
    return std::unexpected("Failed to create send_goal client");
  }

  if (new_entry.send_goal_client == nullptr) [[unlikely]] {
    return std::unexpected("Failed to create send_goal client");
  }

  try {
    const auto get_result_service_name =
        std::format("{}/_action/get_result", action_name);
    new_entry.get_result_client =
        this->create_generic_client(get_result_service_name, get_result_type,
                                    rclcpp::ServicesQoS(), action_group_);
  } catch (const std::exception& ex) {
    return std::unexpected(
        std::format("Failed to create get_result client: {}", ex.what()));
  } catch (...) {
    return std::unexpected("Failed to create get_result client");
  }

  if (new_entry.get_result_client == nullptr) [[unlikely]] {
    return std::unexpected("Failed to create get_result client");
  }

  try {
    const auto cancel_goal_service_name =
        std::format("{}/_action/cancel_goal", action_name);
    new_entry.cancel_goal_client = this->create_generic_client(
        cancel_goal_service_name, kCancelGoalServiceType, rclcpp::ServicesQoS(),
        action_group_);
  } catch (const std::exception& ex) {
    return std::unexpected(
        std::format("Failed to create cancel_goal client: {}", ex.what()));
  } catch (...) {
    return std::unexpected("Failed to create cancel_goal client");
  }

  if (new_entry.cancel_goal_client == nullptr) [[unlikely]] {
    return std::unexpected("Failed to create cancel_goal client");
  }

  try {
    const auto feedback_topic = std::format("{}/_action/feedback", action_name);
    rclcpp::SubscriptionOptions subscription_options;
    subscription_options.callback_group = action_group_;
    new_entry.feedback_subscription = this->create_generic_subscription(
        feedback_topic, feedback_msg_type, rclcpp::QoS(10),
        [this, name = std::string(action_name),
         type = std::string(action_type)](
            std::shared_ptr<rclcpp::SerializedMessage> msg) {
          const auto& serialized = msg->get_rcl_serialized_message();
          HandleFeedbackMessage(
              name, type,
              std::span<const uint8_t>(serialized.buffer,
                                       serialized.buffer_length));
        },
        subscription_options);
  } catch (const std::exception& ex) {
    return std::unexpected(
        std::format("Failed to create feedback subscription: {}", ex.what()));
  } catch (...) {
    return std::unexpected("Failed to create feedback subscription");
  }

  if (new_entry.feedback_subscription == nullptr) [[unlikely]] {
    return std::unexpected("Failed to create feedback subscription");
  }

  const auto [inserted_it, inserted] = generic_action_clients_.emplace(
      std::string(action_name), std::move(new_entry));
  ROSCRAFT_ASSERT(inserted, "Generic action client insertion should succeed!");
  return std::ref(inserted_it->second);
}

bool ActionSendGoalNode::DispatchGenericGoal(const ActionSendGoalCmd& cmd,
                                             std::span<const uint8_t> payload,
                                             GenericActionClientEntry& entry) {
  const auto timeout = ResolveTimeout(cmd.timeout_seconds);
  if (!entry.send_goal_client->wait_for_service(timeout)) {
    const auto it = generic_action_clients_.find(cmd.action_name);
    if (it != generic_action_clients_.end()) {
      generic_action_clients_.erase(it);
    }
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               "Action server unavailable before timeout");
    return false;
  }

  const auto goal_uuid = GenerateGoalUuid();

  auto send_goal_request =
      details::DynamicMessage::Create(entry.send_goal_introspection.request,
                                      details::MessageInitialization::kZero);
  if (!send_goal_request) [[unlikely]] {
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               std::format("Failed to create send_goal request: {}",
                           send_goal_request.error().message));
    return false;
  }

  const auto goal_id_offset =
      FindGoalIdOffset(entry.send_goal_introspection.request);
  if (!goal_id_offset.has_value()) [[unlikely]] {
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               "Failed to locate goal_id field in send_goal request");
    return false;
  }

  auto* request_data = static_cast<uint8_t*>(send_goal_request->Data());
  std::ranges::copy(goal_uuid, request_data + *goal_id_offset);

  const auto goal_member =
      FindGoalMember(entry.send_goal_introspection.request);
  if (!goal_member.has_value()) [[unlikely]] {
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               "Failed to locate goal field in send_goal request");
    return false;
  }

  if (!payload.empty()) {
    if (goal_member->goal_members == nullptr) [[unlikely]] {
      SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
                 "Goal field has no introspection data");
      return false;
    }

    details::MessageIntrospection goal_intro;
    goal_intro.typesupport_library =
        entry.send_goal_introspection.request.typesupport_library;
    goal_intro.message_typesupport =
        entry.send_goal_introspection.request.message_typesupport;
    goal_intro.message_members = goal_member->goal_members;

    auto payload_text = std::string_view(
        std::launder(reinterpret_cast<const char*>(payload.data())),
        payload.size());
    const auto parse_result = details::ParseYamlToMessage(
        payload_text, goal_intro, request_data + goal_member->goal_offset);
    if (!parse_result) [[unlikely]] {
      SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
                 std::format("Failed to parse goal payload: {}",
                             parse_result.error().message));
      return false;
    }
  }

  ActionGoalSession new_session{};
  new_session.request_id = cmd.request_id;
  new_session.action_name = std::string(cmd.action_name);
  new_session.action_type = std::string(cmd.action_type);
  new_session.goal_uuid = goal_uuid;
  new_session.feedback = cmd.feedback;
  new_session.completed = false;

  if (cmd.timeout_seconds > 0.0) {
    const auto timeout_duration =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double>(cmd.timeout_seconds));
    new_session.timeout_timer = this->create_wall_timer(
        timeout_duration,
        [this, request_id = cmd.request_id] { OnGoalTimeout(request_id); },
        action_group_);
  }

  auto cancel_client = entry.cancel_goal_client;
  auto cancel_intro = entry.cancel_goal_introspection;
  auto session_uuid = goal_uuid;
  new_session.cancel_goal = [cancel_client, cancel_intro, session_uuid]() {
    auto cancel_request = details::DynamicMessage::Create(
        cancel_intro.request, details::MessageInitialization::kZero);
    if (!cancel_request) [[unlikely]] {
      return;
    }

    const auto goal_uuid_offset = FindGoalIdOffset(cancel_intro.request);
    if (!goal_uuid_offset.has_value()) [[unlikely]] {
      return;
    }

    auto* cancel_data = static_cast<uint8_t*>(cancel_request->Data());
    std::ranges::copy(session_uuid, cancel_data + *goal_uuid_offset);

    try {
      cancel_client->async_send_request(cancel_request->Data());
    } catch (...) {
    }
  };

  rclcpp::GenericClient::FutureAndRequestId send_goal_future(
      rclcpp::GenericClient::Future{}, 0);
  try {
    send_goal_future =
        entry.send_goal_client->async_send_request(send_goal_request->Data());
  } catch (const std::exception& ex) {
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               std::format("Failed to send action goal: {}", ex.what()));
    return false;
  } catch (...) {
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               "Failed to send action goal");
    return false;
  }

  if (send_goal_future.future.wait_for(timeout) != std::future_status::ready)
      [[unlikely]] {
    entry.send_goal_client->remove_pending_request(send_goal_future.request_id);
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               "Action goal response timeout");
    return false;
  }

  auto send_goal_response = send_goal_future.future.get();
  if (!send_goal_response) [[unlikely]] {
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               "Action goal response was null");
    return false;
  }

  const auto accepted_offset = details::FindMemberOffset(
      *entry.send_goal_introspection.response.message_members, "accepted");
  if (!accepted_offset.has_value()) [[unlikely]] {
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               "Failed to locate accepted field in send_goal response");
    return false;
  }

  const auto* response_data =
      static_cast<const uint8_t*>(send_goal_response.get());
  const bool accepted = *std::launder(
      reinterpret_cast<const bool*>(response_data + *accepted_offset));

  if (!accepted) [[unlikely]] {
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               "Action goal was rejected");
    return false;
  }

  const auto [session_it, inserted] =
      sessions_.emplace(cmd.request_id, std::move(new_session));
  ROSCRAFT_ASSERT(inserted, "Action goal session insertion should succeed");

  auto get_result_request =
      details::DynamicMessage::Create(entry.get_result_introspection.request,
                                      details::MessageInitialization::kZero);
  if (!get_result_request) [[unlikely]] {
    RemoveSession(cmd.request_id);
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               std::format("Failed to create get_result request: {}",
                           get_result_request.error().message));
    return false;
  }

  const auto get_result_uuid_offset =
      FindGoalIdOffset(entry.get_result_introspection.request);
  if (!get_result_uuid_offset.has_value()) [[unlikely]] {
    RemoveSession(cmd.request_id);
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               "Failed to locate goal_id field in get_result request");
    return false;
  }

  auto* get_result_data = static_cast<uint8_t*>(get_result_request->Data());
  std::ranges::copy(goal_uuid, get_result_data + *get_result_uuid_offset);

  try {
    auto get_result_future =
        entry.get_result_client->async_send_request(get_result_request->Data());
    session_it->second.get_result_future = std::move(get_result_future);
  } catch (const std::exception& ex) {
    RemoveSession(cmd.request_id);
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               std::format("Failed to send get_result request: {}", ex.what()));
    return false;
  } catch (...) {
    RemoveSession(cmd.request_id);
    SendResult(cmd.request_id, cmd.action_name, cmd.action_type, false, {},
               "Failed to send get_result request");
    return false;
  }

  return true;
}

void ActionSendGoalNode::HandleFeedbackMessage(
    std::string_view action_name, std::string_view /*action_type*/,
    std::span<const uint8_t> serialized_data) {
  const auto entry_it = generic_action_clients_.find(action_name);
  if (entry_it == generic_action_clients_.end()) [[unlikely]] {
    return;
  }

  auto& entry = entry_it->second;
  const auto& feedback_intro = entry.feedback_message_introspection;

  auto feedback_msg = details::DynamicMessage::Create(
      feedback_intro, details::MessageInitialization::kAll);
  if (!feedback_msg) [[unlikely]] {
    return;
  }

  const auto deserialize_result = details::DeserializeCdrToMessage(
      serialized_data, feedback_intro, feedback_msg->Data());
  if (!deserialize_result) [[unlikely]] {
    return;
  }

  const auto goal_id_offset = FindGoalIdOffset(feedback_intro);
  if (!goal_id_offset.has_value()) [[unlikely]] {
    return;
  }

  const auto* msg_data = static_cast<const uint8_t*>(feedback_msg->Data());
  std::array<uint8_t, 16> feedback_uuid = {};
  std::copy(msg_data + *goal_id_offset, msg_data + *goal_id_offset + 16,
            feedback_uuid.begin());

  ActionGoalSession* matching_session = nullptr;
  for (auto& [request_id, session] : sessions_) {
    if (session.completed || !session.feedback) {
      continue;
    }
    if (session.goal_uuid == feedback_uuid) {
      matching_session = &session;
      break;
    }
  }

  if (matching_session == nullptr) [[unlikely]] {
    return;
  }

  const auto feedback_member = FindFeedbackMember(feedback_intro);
  std::string feedback_text;
  std::vector<uint8_t> feedback_payload;

  if (feedback_member.has_value() &&
      feedback_member->feedback_members != nullptr) {
    details::MessageIntrospection nested_intro;
    nested_intro.typesupport_library = feedback_intro.typesupport_library;
    nested_intro.message_typesupport = feedback_intro.message_typesupport;
    nested_intro.message_members = feedback_member->feedback_members;

    feedback_text =
        details::FormatMessageAsJson(
            nested_intro, msg_data + feedback_member->feedback_offset)
            .value_or("{}");

    auto feedback_bytes = details::SerializeMessageToCdr(
        nested_intro, msg_data + feedback_member->feedback_offset);
    if (feedback_bytes) {
      feedback_payload = std::move(*feedback_bytes);
    }
  }

  SendFeedback(matching_session->request_id, matching_session->action_name,
               matching_session->action_type, feedback_payload, feedback_text);
}

void ActionSendGoalNode::CheckGetResultFutures() {
  std::pmr::vector<uint64_t> completed_sessions{&scratch_arena_};
  completed_sessions.reserve(sessions_.size());

  for (auto& [request_id, session] : sessions_) {
    if (session.completed || !session.get_result_future.has_value()) {
      continue;
    }

    if (session.get_result_future->future.wait_for(std::chrono::seconds(0)) !=
        std::future_status::ready) {
      continue;
    }

    auto response_raw = session.get_result_future->future.get();
    session.get_result_future.reset();

    if (!response_raw) [[unlikely]] {
      session.completed = true;
      SendResult(request_id, session.action_name, session.action_type, false,
                 {}, "Action result response was null");
      completed_sessions.push_back(request_id);
      continue;
    }

    const auto entry_it = generic_action_clients_.find(session.action_name);
    if (entry_it == generic_action_clients_.end()) [[unlikely]] {
      session.completed = true;
      SendResult(request_id, session.action_name, session.action_type, false,
                 {},
                 "Action client entry not found for result deserialization");
      completed_sessions.push_back(request_id);
      continue;
    }

    auto& entry = entry_it->second;
    const auto& get_result_intro = entry.get_result_introspection;

    const auto result_info = FindResultMember(get_result_intro);
    if (!result_info.has_value()) [[unlikely]] {
      session.completed = true;
      SendResult(request_id, session.action_name, session.action_type, false,
                 {}, "Failed to locate result fields in get_result response");
      completed_sessions.push_back(request_id);
      continue;
    }

    const auto* response_data = static_cast<const uint8_t*>(response_raw.get());
    const int8_t status = *std::launder(reinterpret_cast<const int8_t*>(
        response_data + result_info->status_offset));

    auto result_text = std::string(ResultCodeToText(status));
    std::vector<uint8_t> result_payload;

    const bool success = (status == 4);

    if (result_info->result_members != nullptr) {
      details::MessageIntrospection result_intro;
      result_intro.typesupport_library =
          get_result_intro.response.typesupport_library;
      result_intro.message_typesupport =
          get_result_intro.response.message_typesupport;
      result_intro.message_members = result_info->result_members;

      const auto result_text_opt = details::FormatMessageAsJson(
          result_intro, response_data + result_info->result_offset);
      if (result_text_opt) {
        result_text.push_back(' ');
        result_text.append(*result_text_opt);
      }

      auto result_bytes = details::SerializeMessageToCdr(
          result_intro, response_data + result_info->result_offset);
      if (result_bytes) {
        result_payload = std::move(*result_bytes);
      }
    }

    SendResult(request_id, session.action_name, session.action_type, success,
               result_payload, result_text);
    session.completed = true;
    completed_sessions.push_back(request_id);
  }

  for (const auto rid : completed_sessions) {
    RemoveSession(rid);
  }
}

void ActionSendGoalNode::OnGoalTimeout(uint64_t request_id) {
  const auto it = sessions_.find(request_id);
  if (it == sessions_.end()) [[unlikely]] {
    return;
  }

  auto& session = it->second;
  if (session.completed) {
    RemoveSession(request_id);
    return;
  }

  if (session.cancel_goal) {
    try {
      session.cancel_goal();
    } catch (...) {
    }
  }

  SendResult(request_id, session.action_name, session.action_type, false, {},
             "Action goal timed out");
  session.completed = true;
  RemoveSession(request_id);
}

void ActionSendGoalNode::OnPollTimer() {
  DrainActionSendGoalCommands();
  CheckGetResultFutures();
  scratch_arena_.Reset();
}

void ActionSendGoalNode::SendFeedback(uint64_t request_id,
                                      std::string_view action_name,
                                      std::string_view action_type,
                                      std::span<const uint8_t> feedback_payload,
                                      std::string_view feedback_text) {
  auto* const mr = std::pmr::get_default_resource();
  ActionFeedbackCmd cmd(mr);
  cmd.request_id = request_id;
  cmd.action_name = std::pmr::string(action_name, mr);
  cmd.action_type = std::pmr::string(action_type, mr);
  cmd.feedback_payload.assign(feedback_payload.begin(), feedback_payload.end());
  cmd.feedback_text = std::pmr::string(feedback_text, mr);

  outgoing_.get().Enqueue(action_feedback_producer_, std::move(cmd));
}

void ActionSendGoalNode::SendResult(uint64_t request_id,
                                    std::string_view action_name,
                                    std::string_view action_type, bool success,
                                    std::span<const uint8_t> result_payload,
                                    std::string_view result_text) {
  auto* const mr = std::pmr::get_default_resource();
  ActionResultCmd cmd(mr);
  cmd.request_id = request_id;
  cmd.action_name = std::pmr::string(action_name, mr);
  cmd.action_type = std::pmr::string(action_type, mr);
  cmd.success = success;
  cmd.result_payload.assign(result_payload.begin(), result_payload.end());
  cmd.result_text = std::pmr::string(result_text, mr);

  outgoing_.get().Enqueue(action_result_producer_, std::move(cmd));
}

void ActionSendGoalNode::RemoveSession(uint64_t request_id) {
  const auto it = sessions_.find(request_id);
  if (it == sessions_.end()) [[unlikely]] {
    return;
  }

  auto& session = it->second;
  if (session.timeout_timer != nullptr) {
    session.timeout_timer->cancel();
    session.timeout_timer.reset();
  }

  sessions_.erase(it);
}

}  // namespace roscraft::bridge
