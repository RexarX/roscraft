#pragma once

#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

/// @brief Request action endpoint statistics.
struct ActionInfoCmd {
  static constexpr std::string_view kName = "ActionInfoCmd";

  uint64_t request_id = 0;
  std::pmr::string action_name;
  bool include_hidden = false;

  ActionInfoCmd() : ActionInfoCmd(std::pmr::get_default_resource()) {}
  explicit ActionInfoCmd(std::pmr::memory_resource* mr) : action_name(mr) {}
};

/// @brief Request sending an action goal.
struct ActionSendGoalCmd {
  static constexpr std::string_view kName = "ActionSendGoalCmd";

  uint64_t request_id = 0;
  std::pmr::string action_name;
  std::pmr::string action_type;
  std::pmr::vector<uint8_t> goal_payload;
  bool feedback = false;
  double timeout_seconds = 0.0;

  ActionSendGoalCmd() : ActionSendGoalCmd(std::pmr::get_default_resource()) {}
  explicit ActionSendGoalCmd(std::pmr::memory_resource* mr)
      : action_name(mr), action_type(mr), goal_payload(mr) {}
};

/// @brief Response for `ActionInfoCmd`.
struct ActionInfoResponseCmd {
  static constexpr std::string_view kName = "ActionInfoResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string action_name;
  std::pmr::string action_type;
  uint32_t client_count = 0;
  uint32_t server_count = 0;
  uint32_t feedback_publisher_count = 0;
  uint32_t feedback_subscriber_count = 0;
  uint32_t status_publisher_count = 0;
  uint32_t status_subscriber_count = 0;

  ActionInfoResponseCmd()
      : ActionInfoResponseCmd(std::pmr::get_default_resource()) {}
  explicit ActionInfoResponseCmd(std::pmr::memory_resource* mr)
      : action_name(mr), action_type(mr) {}
};

/// @brief Streaming feedback for action goals.
struct ActionFeedbackCmd {
  static constexpr std::string_view kName = "ActionFeedbackCmd";

  uint64_t request_id = 0;
  std::pmr::string action_name;
  std::pmr::string action_type;
  std::pmr::vector<uint8_t> feedback_payload;
  std::pmr::string feedback_text;

  ActionFeedbackCmd() : ActionFeedbackCmd(std::pmr::get_default_resource()) {}
  explicit ActionFeedbackCmd(std::pmr::memory_resource* mr)
      : action_name(mr),
        action_type(mr),
        feedback_payload(mr),
        feedback_text(mr) {}
};

/// @brief Final result for action goals.
struct ActionResultCmd {
  static constexpr std::string_view kName = "ActionResultCmd";

  uint64_t request_id = 0;
  std::pmr::string action_name;
  std::pmr::string action_type;

  std::pmr::vector<uint8_t> result_payload;
  std::pmr::string result_text;

  bool success = false;

  ActionResultCmd() : ActionResultCmd(std::pmr::get_default_resource()) {}
  explicit ActionResultCmd(std::pmr::memory_resource* mr)
      : action_name(mr), action_type(mr), result_payload(mr), result_text(mr) {}
};

}  // namespace roscraft::bridge
