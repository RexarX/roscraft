#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/details/introspection_codec.hpp>

#include <glaze/yaml.hpp>

#include <rclcpp/serialization.hpp>
#include <rclcpp/typesupport_helpers.hpp>

#include <rcpputils/shared_library.hpp>

#include <rosidl_runtime_c/message_type_support_struct.h>
#include <rosidl_runtime_c/service_type_support_struct.h>

#include <rosidl_runtime_cpp/message_initialization.hpp>

#include <rosidl_typesupport_introspection_cpp/field_types.hpp>
#include <rosidl_typesupport_introspection_cpp/identifier.hpp>
#include <rosidl_typesupport_introspection_cpp/message_introspection.hpp>
#include <rosidl_typesupport_introspection_cpp/service_introspection.hpp>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace roscraft::bridge::details {

namespace {

using GenericYamlValue = glz::generic;
using GenericYamlArray = GenericYamlValue::array_t;
using GenericYamlObject = GenericYamlValue::object_t;

using IntrospectionMessageMember = rticpp::MessageMember;
using IntrospectionMessageMembers = rticpp::MessageMembers;
using IntrospectionServiceMembers = rticpp::ServiceMembers;

const std::string kCppTypeSupportIdentifier = "rosidl_typesupport_cpp";

template <typename ValueT>
[[nodiscard]] auto MakeUnexpected(IntrospectionCodecErrorCode code,
                                  std::string message)
    -> IntrospectionCodecResult<ValueT> {
  return std::unexpected(
      IntrospectionCodecError::From(code, std::move(message)));
}

[[nodiscard]] static constexpr auto ToRosInitialization(
    MessageInitialization init) noexcept
    -> rosidl_runtime_cpp::MessageInitialization {
  switch (init) {
    case MessageInitialization::kAll:
      return rosidl_runtime_cpp::MessageInitialization::ALL;
    case MessageInitialization::kZero:
      return rosidl_runtime_cpp::MessageInitialization::ZERO;
    case MessageInitialization::kDefaults:
      return rosidl_runtime_cpp::MessageInitialization::DEFAULTS_ONLY;
    case MessageInitialization::kSkip:
      return rosidl_runtime_cpp::MessageInitialization::SKIP;
    default:
      return rosidl_runtime_cpp::MessageInitialization::ALL;
  }
}

[[nodiscard]] std::string BuildMemberPath(std::string_view parent,
                                          std::string_view member) {
  if (parent.empty()) {
    return std::string(member);
  }

  std::string path(parent);
  if (path != "$") {
    path.push_back('.');
  } else {
    path.clear();
  }
  path.append(member);
  return path;
}

[[nodiscard]] std::string BuildArrayPath(std::string_view parent,
                                         size_t index) {
  return std::format("{}[{}]", parent, index);
}

[[nodiscard]] bool ParseDelayStampOffsets(
    const IntrospectionMessageMembers& message_members,
    DelayStampOffsets& out_offsets) {
  const auto* sec_member = FindMemberByName(message_members, "sec");
  const auto* nanosec_member = FindMemberByName(message_members, "nanosec");
  if (sec_member != nullptr && nanosec_member != nullptr &&
      sec_member->type_id_ == rticpp::ROS_TYPE_INT32 &&
      nanosec_member->type_id_ == rticpp::ROS_TYPE_UINT32 &&
      !sec_member->is_array_ && !nanosec_member->is_array_) {
    out_offsets.message_size = message_members.size_of_;
    out_offsets.uses_header_field = false;
    out_offsets.header_offset = 0;
    out_offsets.stamp_offset = 0;
    out_offsets.sec_offset = sec_member->offset_;
    out_offsets.nanosec_offset = nanosec_member->offset_;
    return true;
  }

  const auto* header_member = FindMemberByName(message_members, "header");
  if (header_member == nullptr ||
      header_member->type_id_ != rticpp::ROS_TYPE_MESSAGE ||
      header_member->is_array_ || header_member->members_ == nullptr) {
    return false;
  }

  const auto* header_members = ToIntrospectionMembers(header_member->members_);
  if (header_members == nullptr ||
      header_members->message_namespace_ == nullptr ||
      header_members->message_name_ == nullptr ||
      std::string_view(header_members->message_namespace_) != "std_msgs::msg" ||
      std::string_view(header_members->message_name_) != "Header") {
    return false;
  }

  const auto* stamp_member = FindMemberByName(*header_members, "stamp");
  if (stamp_member == nullptr ||
      stamp_member->type_id_ != rticpp::ROS_TYPE_MESSAGE ||
      stamp_member->is_array_ || stamp_member->members_ == nullptr) {
    return false;
  }

  const auto* time_members = ToIntrospectionMembers(stamp_member->members_);
  if (time_members == nullptr || time_members->message_namespace_ == nullptr ||
      time_members->message_name_ == nullptr ||
      std::string_view(time_members->message_namespace_) !=
          "builtin_interfaces::msg" ||
      std::string_view(time_members->message_name_) != "Time") {
    return false;
  }

  const auto* time_sec_member = FindMemberByName(*time_members, "sec");
  const auto* time_nanosec_member = FindMemberByName(*time_members, "nanosec");
  if (time_sec_member == nullptr || time_nanosec_member == nullptr ||
      time_sec_member->type_id_ != rticpp::ROS_TYPE_INT32 ||
      time_nanosec_member->type_id_ != rticpp::ROS_TYPE_UINT32 ||
      time_sec_member->is_array_ || time_nanosec_member->is_array_) {
    return false;
  }

  out_offsets.message_size = message_members.size_of_;
  out_offsets.uses_header_field = true;
  out_offsets.header_offset = header_member->offset_;
  out_offsets.stamp_offset = stamp_member->offset_;
  out_offsets.sec_offset = time_sec_member->offset_;
  out_offsets.nanosec_offset = time_nanosec_member->offset_;
  return true;
}

[[nodiscard]] constexpr double ToStampSeconds(int32_t sec,
                                              uint32_t nanosec) noexcept {
  constexpr double kNanosPerSecond = 1'000'000'000.0;
  return static_cast<double>(sec) +
         static_cast<double>(nanosec) / kNanosPerSecond;
}

[[nodiscard]] auto ParseBoolToken(std::string_view token)
    -> std::optional<bool> {
  if (token == "true" || token == "1") {
    return true;
  }
  if (token == "false" || token == "0") {
    return false;
  }

  return std::nullopt;
}

[[nodiscard]] auto ParseDoubleString(std::string_view token)
    -> std::optional<double> {
  if (token.empty()) [[unlikely]] {
    return std::nullopt;
  }

  double value = 0.0;
  const auto* begin = token.data();
  const auto* end = token.data() + token.size();
  const auto [ptr, ec] = std::from_chars(begin, end, value);
  if (ec != std::errc{} || ptr != end || !std::isfinite(value)) [[unlikely]] {
    return std::nullopt;
  }

  return value;
}

[[nodiscard]] auto ParseDoubleValue(const GenericYamlValue& value)
    -> std::optional<double> {
  if (value.is_number()) {
    const double number = value.get_number();
    if (!std::isfinite(number)) [[unlikely]] {
      return std::nullopt;
    }
    return number;
  }

  if (value.is_string()) {
    return ParseDoubleString(value.get_string());
  }

  return std::nullopt;
}

template <typename IntT>
[[nodiscard]] auto ParseIntegerValue(const GenericYamlValue& value)
    -> std::optional<IntT> {
  static_assert(std::integral<IntT>);

  if (value.is_number()) {
    const double number = value.get_number();
    constexpr double kIntegerTolerance = 1e-6;
    if (!std::isfinite(number) || std::abs(number - std::round(number)) >
                                      kIntegerTolerance) [[unlikely]] {
      return std::nullopt;
    }

    constexpr auto kMinAsDouble =
        static_cast<double>(std::numeric_limits<IntT>::lowest());
    constexpr auto kMaxAsDouble =
        static_cast<double>(std::numeric_limits<IntT>::max());
    if (number < kMinAsDouble || number > kMaxAsDouble) [[unlikely]] {
      return std::nullopt;
    }

    return static_cast<IntT>(std::round(number));
  }

  if (value.is_string()) {
    IntT parsed{};
    const auto* begin = value.get_string().data();
    const auto* end = value.get_string().data() + value.get_string().size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc{} || ptr != end) [[unlikely]] {
      return std::nullopt;
    }

    return parsed;
  }

  return std::nullopt;
}

[[nodiscard]] auto ParseStringValue(const GenericYamlValue& value)
    -> std::optional<std::string> {
  if (!value.is_string()) [[unlikely]] {
    return std::nullopt;
  }

  return value.get_string();
}

[[nodiscard]] auto ParseBooleanValue(const GenericYamlValue& value)
    -> std::optional<bool> {
  if (value.is_boolean()) {
    return value.get_boolean();
  }

  if (value.is_number()) {
    if (const auto parsed = ParseIntegerValue<int64_t>(value);
        parsed.has_value()) {
      if (*parsed == 0) {
        return false;
      }
      if (*parsed == 1) {
        return true;
      }
    }
    return std::nullopt;
  }

  if (value.is_string()) {
    std::string token(value.get_string());
    std::ranges::transform(token, token.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });
    return ParseBoolToken(token);
  }

  return std::nullopt;
}

[[nodiscard]] auto ParseAsciiWString(const GenericYamlValue& value)
    -> std::optional<std::u16string> {
  const auto parsed = ParseStringValue(value);
  if (!parsed.has_value()) [[unlikely]] {
    return std::nullopt;
  }

  std::u16string converted;
  converted.reserve(parsed->size());
  for (unsigned char ch : *parsed) {
    if (ch > 0x7F) [[unlikely]] {
      return std::nullopt;
    }
    converted.push_back(static_cast<char16_t>(ch));
  }
  return converted;
}

[[nodiscard]] constexpr uint8_t NormalizeTypeId(uint8_t type_id) noexcept {
  switch (type_id) {
    case rticpp::ROS_TYPE_BOOL:
      return rticpp::ROS_TYPE_BOOLEAN;
    case rticpp::ROS_TYPE_BYTE:
      return rticpp::ROS_TYPE_OCTET;
    case rticpp::ROS_TYPE_FLOAT32:
      return rticpp::ROS_TYPE_FLOAT;
    case rticpp::ROS_TYPE_FLOAT64:
      return rticpp::ROS_TYPE_DOUBLE;
    default:
      return type_id;
  }
}

[[nodiscard]] auto AssignScalarValue(const GenericYamlValue& value,
                                     uint8_t type_id, void* storage,
                                     std::string_view field_path)
    -> IntrospectionCodecResult<void> {
  if (storage == nullptr) [[unlikely]] {
    return MakeUnexpected<void>(
        IntrospectionCodecErrorCode::kInvalidFieldValue,
        std::format("Field '{}' storage pointer is null", field_path));
  }

  switch (NormalizeTypeId(type_id)) {
    case rticpp::ROS_TYPE_BOOLEAN: {
      const auto parsed = ParseBooleanValue(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects a bool value", field_path));
      }
      *static_cast<bool*>(storage) = *parsed;
      return {};
    }

    case rticpp::ROS_TYPE_INT8: {
      const auto parsed = ParseIntegerValue<int8_t>(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects an int8 value", field_path));
      }
      *static_cast<int8_t*>(storage) = *parsed;
      return {};
    }

    case rticpp::ROS_TYPE_UINT8:
    case rticpp::ROS_TYPE_OCTET:
    case rticpp::ROS_TYPE_CHAR: {
      if (value.is_string()) {
        const auto parsed_string = value.get_string();
        if (parsed_string.size() == 1) {
          *static_cast<uint8_t*>(storage) =
              static_cast<uint8_t>(parsed_string.front());
          return {};
        }
      }

      const auto parsed = ParseIntegerValue<uint8_t>(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects a uint8 value", field_path));
      }
      *static_cast<uint8_t*>(storage) = *parsed;
      return {};
    }

    case rticpp::ROS_TYPE_INT16: {
      const auto parsed = ParseIntegerValue<int16_t>(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects an int16 value", field_path));
      }
      *static_cast<int16_t*>(storage) = *parsed;
      return {};
    }

    case rticpp::ROS_TYPE_UINT16:
    case rticpp::ROS_TYPE_WCHAR: {
      if (value.is_string()) {
        const auto parsed_string = value.get_string();
        if (parsed_string.size() == 1) {
          *static_cast<uint16_t*>(storage) = static_cast<uint16_t>(
              static_cast<unsigned char>(parsed_string.front()));
          return {};
        }
      }

      const auto parsed = ParseIntegerValue<uint16_t>(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects a uint16 value", field_path));
      }
      *static_cast<uint16_t*>(storage) = *parsed;
      return {};
    }

    case rticpp::ROS_TYPE_INT32: {
      const auto parsed = ParseIntegerValue<int32_t>(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects an int32 value", field_path));
      }
      *static_cast<int32_t*>(storage) = *parsed;
      return {};
    }

    case rticpp::ROS_TYPE_UINT32: {
      const auto parsed = ParseIntegerValue<uint32_t>(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects a uint32 value", field_path));
      }
      *static_cast<uint32_t*>(storage) = *parsed;
      return {};
    }

    case rticpp::ROS_TYPE_INT64: {
      const auto parsed = ParseIntegerValue<int64_t>(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects an int64 value", field_path));
      }
      *static_cast<int64_t*>(storage) = *parsed;
      return {};
    }

    case rticpp::ROS_TYPE_UINT64: {
      const auto parsed = ParseIntegerValue<uint64_t>(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects a uint64 value", field_path));
      }
      *static_cast<uint64_t*>(storage) = *parsed;
      return {};
    }

    case rticpp::ROS_TYPE_FLOAT: {
      const auto parsed = ParseDoubleValue(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects a float value", field_path));
      }
      *static_cast<float*>(storage) = static_cast<float>(*parsed);
      return {};
    }

    case rticpp::ROS_TYPE_DOUBLE: {
      const auto parsed = ParseDoubleValue(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects a double value", field_path));
      }
      *static_cast<double*>(storage) = *parsed;
      return {};
    }

    case rticpp::ROS_TYPE_LONG_DOUBLE: {
      const auto parsed = ParseDoubleValue(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects a long double value", field_path));
      }
      *static_cast<long double*>(storage) = static_cast<long double>(*parsed);
      return {};
    }

    case rticpp::ROS_TYPE_STRING: {
      const auto parsed = ParseStringValue(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects a string value", field_path));
      }
      static_cast<std::string*>(storage)->assign(*parsed);
      return {};
    }

    case rticpp::ROS_TYPE_WSTRING: {
      const auto parsed = ParseAsciiWString(value);
      if (!parsed.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' expects an ASCII string value for wstring",
                        field_path));
      }
      static_cast<std::u16string*>(storage)->assign(*parsed);
      return {};
    }

    default:
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kUnsupportedFieldType,
          std::format("Field '{}' uses unsupported type id {}", field_path,
                      static_cast<unsigned>(type_id)));
  }
}

[[nodiscard]] auto AssignArrayValue(const GenericYamlArray& values,
                                    const IntrospectionMessageMember& member,
                                    void* member_storage,
                                    std::string_view field_path)
    -> IntrospectionCodecResult<void>;

[[nodiscard]] auto ParseMessageValue(const GenericYamlValue& value,
                                     const IntrospectionMessageMembers& members,
                                     void* message_storage,
                                     std::string_view message_path)
    -> IntrospectionCodecResult<void>;

[[nodiscard]] auto AssignMemberValue(const GenericYamlValue& value,
                                     const IntrospectionMessageMember& member,
                                     void* message_storage,
                                     std::string_view field_path)
    -> IntrospectionCodecResult<void> {
  if (message_storage == nullptr) [[unlikely]] {
    return MakeUnexpected<void>(
        IntrospectionCodecErrorCode::kInvalidFieldValue,
        std::format("Message storage is null for field '{}'", field_path));
  }

  auto* field_storage = static_cast<void*>(
      static_cast<uint8_t*>(message_storage) + member.offset_);

  if (member.is_array_) {
    if (!value.is_array()) [[unlikely]] {
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kInvalidFieldValue,
          std::format("Field '{}' expects an array value", field_path));
    }

    return AssignArrayValue(value.get_array(), member, field_storage,
                            field_path);
  }

  if (member.type_id_ == rticpp::ROS_TYPE_MESSAGE) {
    const auto* nested_members = ToIntrospectionMembers(member.members_);
    if (nested_members == nullptr) [[unlikely]] {
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kInvalidTypeSupport,
          std::format("Field '{}' nested message has invalid type support",
                      field_path));
    }

    return ParseMessageValue(value, *nested_members, field_storage, field_path);
  }

  return AssignScalarValue(value, member.type_id_, field_storage, field_path);
}

[[nodiscard]] auto AssignArrayElementValue(
    const GenericYamlValue& value, const IntrospectionMessageMember& member,
    void* member_storage, size_t index, std::string_view field_path)
    -> IntrospectionCodecResult<void> {
  if (member.type_id_ == rticpp::ROS_TYPE_MESSAGE) {
    if (member.get_function == nullptr) [[unlikely]] {
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kUnsupportedFieldType,
          std::format("Field '{}' does not expose array element accessor",
                      field_path));
    }

    const auto* nested_members = ToIntrospectionMembers(member.members_);
    if (nested_members == nullptr) [[unlikely]] {
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kInvalidTypeSupport,
          std::format("Field '{}' nested message has invalid type support",
                      field_path));
    }

    void* nested_storage = member.get_function(member_storage, index);
    if (nested_storage == nullptr) [[unlikely]] {
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kInvalidFieldValue,
          std::format("Field '{}' array element pointer is null", field_path));
    }

    return ParseMessageValue(value, *nested_members, nested_storage,
                             field_path);
  }

  if (member.assign_function != nullptr) {
    switch (NormalizeTypeId(member.type_id_)) {
      case rticpp::ROS_TYPE_BOOLEAN: {
        const auto parsed = ParseBooleanValue(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects a bool value", field_path));
        }
        bool assign_value = *parsed;
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_INT8: {
        const auto parsed = ParseIntegerValue<int8_t>(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects an int8 value", field_path));
        }
        int8_t assign_value = *parsed;
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_UINT8:
      case rticpp::ROS_TYPE_OCTET:
      case rticpp::ROS_TYPE_CHAR: {
        const auto parsed = ParseIntegerValue<uint8_t>(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects a uint8 value", field_path));
        }
        uint8_t assign_value = *parsed;
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_INT16: {
        const auto parsed = ParseIntegerValue<int16_t>(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects an int16 value", field_path));
        }
        int16_t assign_value = *parsed;
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_UINT16:
      case rticpp::ROS_TYPE_WCHAR: {
        const auto parsed = ParseIntegerValue<uint16_t>(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects a uint16 value", field_path));
        }
        uint16_t assign_value = *parsed;
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_INT32: {
        const auto parsed = ParseIntegerValue<int32_t>(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects an int32 value", field_path));
        }
        int32_t assign_value = *parsed;
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_UINT32: {
        const auto parsed = ParseIntegerValue<uint32_t>(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects a uint32 value", field_path));
        }
        uint32_t assign_value = *parsed;
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_INT64: {
        const auto parsed = ParseIntegerValue<int64_t>(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects an int64 value", field_path));
        }
        int64_t assign_value = *parsed;
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_UINT64: {
        const auto parsed = ParseIntegerValue<uint64_t>(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects a uint64 value", field_path));
        }
        uint64_t assign_value = *parsed;
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_FLOAT: {
        const auto parsed = ParseDoubleValue(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects a float value", field_path));
        }
        float assign_value = static_cast<float>(*parsed);
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_DOUBLE: {
        const auto parsed = ParseDoubleValue(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects a double value", field_path));
        }
        double assign_value = *parsed;
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_LONG_DOUBLE: {
        const auto parsed = ParseDoubleValue(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects a long double value",
                          field_path));
        }
        long double assign_value = static_cast<long double>(*parsed);
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_STRING: {
        const auto parsed = ParseStringValue(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format("Field '{}' expects a string value", field_path));
        }
        std::string assign_value = *parsed;
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      case rticpp::ROS_TYPE_WSTRING: {
        const auto parsed = ParseAsciiWString(value);
        if (!parsed.has_value()) [[unlikely]] {
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kInvalidFieldValue,
              std::format(
                  "Field '{}' expects an ASCII string value for wstring",
                  field_path));
        }
        std::u16string assign_value = *parsed;
        member.assign_function(member_storage, index, &assign_value);
        return {};
      }

      default:
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kUnsupportedFieldType,
            std::format("Field '{}' uses unsupported type id {}", field_path,
                        static_cast<unsigned>(member.type_id_)));
    }
  }

  if (member.get_function == nullptr) [[unlikely]] {
    return MakeUnexpected<void>(
        IntrospectionCodecErrorCode::kUnsupportedFieldType,
        std::format("Field '{}' does not expose mutable array element accessor",
                    field_path));
  }

  void* element_storage = member.get_function(member_storage, index);
  if (element_storage == nullptr) [[unlikely]] {
    return MakeUnexpected<void>(
        IntrospectionCodecErrorCode::kInvalidFieldValue,
        std::format("Field '{}' array element pointer is null", field_path));
  }

  return AssignScalarValue(value, member.type_id_, element_storage, field_path);
}

[[nodiscard]] auto AssignArrayValue(const GenericYamlArray& values,
                                    const IntrospectionMessageMember& member,
                                    void* member_storage,
                                    std::string_view field_path)
    -> IntrospectionCodecResult<void> {
  const size_t input_size = values.size();

  if (!member.is_upper_bound_ && member.array_size_ > 0 &&
      member.resize_function == nullptr) {
    if (input_size != member.array_size_) [[unlikely]] {
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kArraySizeMismatch,
          std::format("Field '{}' expects exactly {} elements, got {}",
                      field_path, member.array_size_, input_size));
    }
  } else if (!member.is_upper_bound_ && member.array_size_ > 0 &&
             member.resize_function != nullptr) {
    if (input_size != member.array_size_) [[unlikely]] {
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kArraySizeMismatch,
          std::format("Field '{}' expects exactly {} elements, got {}",
                      field_path, member.array_size_, input_size));
    }
  } else {
    if (member.is_upper_bound_ && member.array_size_ > 0 &&
        input_size > member.array_size_) [[unlikely]] {
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kArraySizeMismatch,
          std::format("Field '{}' allows at most {} elements, got {}",
                      field_path, member.array_size_, input_size));
    }

    if (member.resize_function != nullptr) {
      member.resize_function(member_storage, input_size);
    }
  }

  for (size_t index = 0; index < input_size; ++index) {
    const auto value_path = BuildArrayPath(field_path, index);
    auto assigned = AssignArrayElementValue(values[index], member,
                                            member_storage, index, value_path);
    if (!assigned) {
      return assigned;
    }
  }

  return {};
}

[[nodiscard]] auto ParseMessageValue(const GenericYamlValue& value,
                                     const IntrospectionMessageMembers& members,
                                     void* message_storage,
                                     std::string_view message_path)
    -> IntrospectionCodecResult<void> {
  if (message_storage == nullptr) [[unlikely]] {
    return MakeUnexpected<void>(
        IntrospectionCodecErrorCode::kInvalidFieldValue,
        std::format("Message storage for '{}' is null", message_path));
  }

  std::string message_type_name;
  if (members.message_namespace_ != nullptr &&
      members.message_name_ != nullptr) {
    message_type_name = std::string(members.message_namespace_);
    message_type_name += "::";
    message_type_name += members.message_name_;
  }

  if (value.is_object()) {
    const GenericYamlObject& object = value.get_object();

    for (const auto& [field_name, field_value] : object) {
      static_cast<void>(field_value);
      const auto* member = FindMemberByName(members, field_name);
      if (member == nullptr) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kUnknownField,
            std::format(
                "Unknown field '{}' in message '{}'", field_name,
                message_type_name.empty() ? message_path : message_type_name));
      }
    }

    for (uint32_t index = 0; index < members.member_count_; ++index) {
      const auto& member = members.members_[index];
      if (member.name_ == nullptr) {
        continue;
      }

      const auto field_it = object.find(member.name_);
      if (field_it == object.end()) {
        continue;
      }

      const auto field_path = BuildMemberPath(message_path, member.name_);
      auto assign_result = AssignMemberValue(field_it->second, member,
                                             message_storage, field_path);
      if (!assign_result) {
        return assign_result;
      }
    }

    return {};
  }

  if (members.member_count_ == 1) {
    const auto& member = members.members_[0];
    if (member.name_ == nullptr) [[unlikely]] {
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kInvalidTypeSupport,
          std::format("Single-field message '{}' has unnamed member",
                      message_path));
    }

    const auto field_path = BuildMemberPath(message_path, member.name_);
    return AssignMemberValue(value, member, message_storage, field_path);
  }

  if (members.member_count_ == 0 && value.is_null()) {
    return {};
  }

  return MakeUnexpected<void>(
      IntrospectionCodecErrorCode::kInvalidYamlShape,
      std::format("Message '{}' expects a mapping payload", message_path));
}

[[nodiscard]] std::string EscapeJsonString(std::string_view text) {
  std::string escaped;
  escaped.reserve(text.size() * 2);
  for (const unsigned char ch : text) {
    switch (ch) {
      case '"':
        escaped.append("\\\"");
        break;
      case '\\':
        escaped.append("\\\\");
        break;
      case '\b':
        escaped.append("\\b");
        break;
      case '\f':
        escaped.append("\\f");
        break;
      case '\n':
        escaped.append("\\n");
        break;
      case '\r':
        escaped.append("\\r");
        break;
      case '\t':
        escaped.append("\\t");
        break;
      default:
        if (ch < 0x20) {
          escaped.append(std::format("\\u{:04x}", static_cast<unsigned>(ch)));
        } else {
          escaped.push_back(static_cast<char>(ch));
        }
        break;
    }
  }
  return escaped;
}

[[nodiscard]] auto WStringToAscii(const std::u16string& value)
    -> std::optional<std::string> {
  std::string converted;
  converted.reserve(value.size());
  for (const auto ch : value) {
    if (ch > 0x7F) {
      return std::nullopt;
    }
    converted.push_back(static_cast<char>(ch));
  }
  return converted;
}

void AppendJsonStringLiteral(std::string_view text, std::string& out) {
  out.push_back('"');
  out.append(EscapeJsonString(text));
  out.push_back('"');
}

[[nodiscard]] auto AppendScalarAsJson(uint8_t type_id, const void* storage,
                                      std::string& out,
                                      std::string_view field_path)
    -> IntrospectionCodecResult<void> {
  if (storage == nullptr) [[unlikely]] {
    return MakeUnexpected<void>(
        IntrospectionCodecErrorCode::kInvalidFieldValue,
        std::format("Field '{}' storage pointer is null", field_path));
  }

  switch (NormalizeTypeId(type_id)) {
    case rticpp::ROS_TYPE_BOOLEAN:
      out.append(*static_cast<const bool*>(storage) ? "true" : "false");
      return {};

    case rticpp::ROS_TYPE_INT8:
      out.append(std::to_string(*static_cast<const int8_t*>(storage)));
      return {};

    case rticpp::ROS_TYPE_UINT8:
    case rticpp::ROS_TYPE_OCTET:
    case rticpp::ROS_TYPE_CHAR:
      out.append(std::to_string(*static_cast<const uint8_t*>(storage)));
      return {};

    case rticpp::ROS_TYPE_INT16:
      out.append(std::to_string(*static_cast<const int16_t*>(storage)));
      return {};

    case rticpp::ROS_TYPE_UINT16:
    case rticpp::ROS_TYPE_WCHAR:
      out.append(std::to_string(*static_cast<const uint16_t*>(storage)));
      return {};

    case rticpp::ROS_TYPE_INT32:
      out.append(std::to_string(*static_cast<const int32_t*>(storage)));
      return {};

    case rticpp::ROS_TYPE_UINT32:
      out.append(std::to_string(*static_cast<const uint32_t*>(storage)));
      return {};

    case rticpp::ROS_TYPE_INT64:
      out.append(std::to_string(*static_cast<const int64_t*>(storage)));
      return {};

    case rticpp::ROS_TYPE_UINT64:
      out.append(std::to_string(*static_cast<const uint64_t*>(storage)));
      return {};

    case rticpp::ROS_TYPE_FLOAT:
      out.append(std::to_string(*static_cast<const float*>(storage)));
      return {};

    case rticpp::ROS_TYPE_DOUBLE:
      out.append(std::to_string(*static_cast<const double*>(storage)));
      return {};

    case rticpp::ROS_TYPE_LONG_DOUBLE:
      out.append(std::to_string(
          static_cast<double>(*static_cast<const long double*>(storage))));
      return {};

    case rticpp::ROS_TYPE_STRING:
      AppendJsonStringLiteral(*static_cast<const std::string*>(storage), out);
      return {};

    case rticpp::ROS_TYPE_WSTRING: {
      const auto converted =
          WStringToAscii(*static_cast<const std::u16string*>(storage));
      if (!converted.has_value()) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' contains non-ASCII data in wstring value",
                        field_path));
      }
      AppendJsonStringLiteral(*converted, out);
      return {};
    }

    default:
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kUnsupportedFieldType,
          std::format("Field '{}' uses unsupported type id {}", field_path,
                      static_cast<unsigned>(type_id)));
  }
}

[[nodiscard]] auto AppendMessageAsJson(
    const IntrospectionMessageMembers& members, const void* message_storage,
    std::string& out, std::string_view message_path)
    -> IntrospectionCodecResult<void>;

[[nodiscard]] auto AppendArrayAsJson(const IntrospectionMessageMember& member,
                                     const void* member_storage,
                                     std::string& out,
                                     std::string_view field_path)
    -> IntrospectionCodecResult<void> {
  size_t size = member.array_size_;
  if (member.size_function != nullptr) {
    size = member.size_function(member_storage);
  }

  out.push_back('[');
  for (size_t index = 0; index < size; ++index) {
    if (index > 0) {
      out.push_back(',');
    }

    const auto value_path = BuildArrayPath(field_path, index);
    if (member.type_id_ == rticpp::ROS_TYPE_MESSAGE) {
      if (member.get_const_function == nullptr) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kUnsupportedFieldType,
            std::format("Field '{}' does not expose const array accessor",
                        value_path));
      }

      const auto* nested_members = ToIntrospectionMembers(member.members_);
      if (nested_members == nullptr) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidTypeSupport,
            std::format("Field '{}' nested message has invalid type support",
                        value_path));
      }

      const void* nested_storage =
          member.get_const_function(member_storage, index);
      if (nested_storage == nullptr) [[unlikely]] {
        return MakeUnexpected<void>(
            IntrospectionCodecErrorCode::kInvalidFieldValue,
            std::format("Field '{}' array element pointer is null",
                        value_path));
      }

      auto nested_result =
          AppendMessageAsJson(*nested_members, nested_storage, out, value_path);
      if (!nested_result) {
        return nested_result;
      }
      continue;
    }

    if (member.fetch_function != nullptr) {
      switch (NormalizeTypeId(member.type_id_)) {
        case rticpp::ROS_TYPE_BOOLEAN: {
          bool element{};
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_INT8: {
          int8_t element{};
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_UINT8:
        case rticpp::ROS_TYPE_OCTET:
        case rticpp::ROS_TYPE_CHAR: {
          uint8_t element{};
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_INT16: {
          int16_t element{};
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_UINT16:
        case rticpp::ROS_TYPE_WCHAR: {
          uint16_t element{};
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_INT32: {
          int32_t element{};
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_UINT32: {
          uint32_t element{};
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_INT64: {
          int64_t element{};
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_UINT64: {
          uint64_t element{};
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_FLOAT: {
          float element{};
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_DOUBLE: {
          double element{};
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_LONG_DOUBLE: {
          long double element{};
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_STRING: {
          std::string element;
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        case rticpp::ROS_TYPE_WSTRING: {
          std::u16string element;
          member.fetch_function(member_storage, index, &element);
          auto append_result =
              AppendScalarAsJson(member.type_id_, &element, out, value_path);
          if (!append_result) {
            return append_result;
          }
          continue;
        }

        default:
          return MakeUnexpected<void>(
              IntrospectionCodecErrorCode::kUnsupportedFieldType,
              std::format("Field '{}' uses unsupported type id {}", value_path,
                          static_cast<unsigned>(member.type_id_)));
      }
    }

    if (member.get_const_function == nullptr) [[unlikely]] {
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kUnsupportedFieldType,
          std::format("Field '{}' does not expose const array accessor",
                      value_path));
    }

    const void* element = member.get_const_function(member_storage, index);
    if (element == nullptr) [[unlikely]] {
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kInvalidFieldValue,
          std::format("Field '{}' array element pointer is null", value_path));
    }

    auto append_result =
        AppendScalarAsJson(member.type_id_, element, out, value_path);
    if (!append_result) {
      return append_result;
    }
  }

  out.push_back(']');
  return {};
}

[[nodiscard]] auto AppendMemberAsJson(const IntrospectionMessageMember& member,
                                      const void* message_storage,
                                      std::string& out,
                                      std::string_view field_path)
    -> IntrospectionCodecResult<void> {
  const auto* field_storage = static_cast<const void*>(
      static_cast<const uint8_t*>(message_storage) + member.offset_);

  if (member.is_array_) {
    return AppendArrayAsJson(member, field_storage, out, field_path);
  }

  if (member.type_id_ == rticpp::ROS_TYPE_MESSAGE) {
    const auto* nested_members = ToIntrospectionMembers(member.members_);
    if (nested_members == nullptr) [[unlikely]] {
      return MakeUnexpected<void>(
          IntrospectionCodecErrorCode::kInvalidTypeSupport,
          std::format("Field '{}' nested message has invalid type support",
                      field_path));
    }

    return AppendMessageAsJson(*nested_members, field_storage, out, field_path);
  }

  return AppendScalarAsJson(member.type_id_, field_storage, out, field_path);
}

[[nodiscard]] auto AppendMessageAsJson(
    const IntrospectionMessageMembers& members, const void* message_storage,
    std::string& out, std::string_view message_path)
    -> IntrospectionCodecResult<void> {
  out.push_back('{');

  bool first = true;
  for (uint32_t index = 0; index < members.member_count_; ++index) {
    const auto& member = members.members_[index];
    if (member.name_ == nullptr) {
      continue;
    }

    if (!first) {
      out.push_back(',');
    }
    first = false;

    AppendJsonStringLiteral(member.name_, out);
    out.push_back(':');

    const auto field_path = BuildMemberPath(message_path, member.name_);
    auto append_result =
        AppendMemberAsJson(member, message_storage, out, field_path);
    if (!append_result) {
      return append_result;
    }
  }

  out.push_back('}');
  return {};
}

}  // namespace

auto ToIntrospectionMembers(const rosidl_message_type_support_t* type_support)
    -> const rticpp::MessageMembers_s* {
  if (type_support == nullptr ||
      type_support->typesupport_identifier == nullptr ||
      type_support->data == nullptr) [[unlikely]] {
    return nullptr;
  }

  if (std::string_view(type_support->typesupport_identifier) !=
      rticpp::typesupport_identifier) [[unlikely]] {
    return nullptr;
  }

  return static_cast<const IntrospectionMessageMembers*>(type_support->data);
}

auto FindMemberByName(const rticpp::MessageMembers_s& members,
                      std::string_view name) -> const rticpp::MessageMember_s* {
  for (uint32_t index = 0; index < members.member_count_; ++index) {
    const auto& member = members.members_[index];
    if (member.name_ == nullptr) {
      continue;
    }

    if (std::string_view(member.name_) == name) {
      return &member;
    }
  }

  return nullptr;
}

auto FindMemberOffset(const rticpp::MessageMembers_s& members,
                      std::string_view name) -> std::optional<size_t> {
  const auto* member = FindMemberByName(members, name);
  if (member == nullptr) {
    return std::nullopt;
  }

  return member->offset_;
}

auto DynamicMessage::Create(const MessageIntrospection& introspection,
                            MessageInitialization initialization)
    -> IntrospectionCodecResult<DynamicMessage> {
  if (introspection.message_members == nullptr ||
      introspection.message_members->init_function == nullptr ||
      introspection.message_members->fini_function == nullptr ||
      introspection.message_members->size_of_ == 0) [[unlikely]] {
    return MakeUnexpected<DynamicMessage>(
        IntrospectionCodecErrorCode::kInvalidTypeSupport,
        "Message introspection is incomplete");
  }

  std::vector<uint8_t> storage(introspection.message_members->size_of_, 0);
  introspection.message_members->init_function(
      storage.data(), ToRosInitialization(initialization));

  return DynamicMessage(introspection.message_members, std::move(storage));
}

void DynamicMessage::Finalize() {
  if (message_members_ == nullptr || storage_.empty()) [[unlikely]] {
    return;
  }

  message_members_->fini_function(storage_.data());
  storage_.clear();
  message_members_ = nullptr;
}

auto LoadMessageIntrospection(std::string_view message_type)
    -> IntrospectionCodecResult<MessageIntrospection> {
  MessageIntrospection output;

  const rosidl_message_type_support_t* message_type_support_cpp = nullptr;
  const rosidl_message_type_support_t* message_type_support_intro = nullptr;
  const std::string message_type_string(message_type);

  try {
    output.typesupport_library = rclcpp::get_typesupport_library(
        message_type_string, kCppTypeSupportIdentifier);
    message_type_support_cpp = rclcpp::get_message_typesupport_handle(
        message_type_string, kCppTypeSupportIdentifier,
        *output.typesupport_library);
    message_type_support_intro = ::get_message_typesupport_handle(
        message_type_support_cpp, rticpp::typesupport_identifier);
  } catch (const std::exception& ex) {
    return MakeUnexpected<MessageIntrospection>(
        IntrospectionCodecErrorCode::kTypeSupportLoadFailed,
        std::format("Failed to load message type support for '{}': {}",
                    message_type, ex.what()));
  } catch (...) {
    return MakeUnexpected<MessageIntrospection>(
        IntrospectionCodecErrorCode::kTypeSupportLoadFailed,
        std::format("Failed to load message type support for '{}'",
                    message_type));
  }

  output.message_typesupport = message_type_support_cpp;
  output.message_members = ToIntrospectionMembers(message_type_support_intro);
  if (output.message_members == nullptr) [[unlikely]] {
    return MakeUnexpected<MessageIntrospection>(
        IntrospectionCodecErrorCode::kInvalidTypeSupport,
        std::format(
            "Message '{}' does not expose introspection message members",
            message_type));
  }

  return output;
}

auto LoadServiceIntrospection(std::string_view service_type)
    -> IntrospectionCodecResult<ServiceIntrospection> {
  ServiceIntrospection output;

  const rosidl_service_type_support_t* service_type_support_intro = nullptr;
  const rosidl_message_type_support_t* request_message_typesupport_cpp =
      nullptr;
  const rosidl_message_type_support_t* response_message_typesupport_cpp =
      nullptr;
  const rosidl_message_type_support_t* request_message_typesupport_intro =
      nullptr;
  const rosidl_message_type_support_t* response_message_typesupport_intro =
      nullptr;
  const std::string service_type_string(service_type);
  try {
    output.typesupport_library = rclcpp::get_typesupport_library(
        service_type_string, kCppTypeSupportIdentifier);

    output.service_typesupport = rclcpp::get_service_typesupport_handle(
        service_type_string, kCppTypeSupportIdentifier,
        *output.typesupport_library);
    service_type_support_intro = ::get_service_typesupport_handle(
        output.service_typesupport, rticpp::typesupport_identifier);
  } catch (const std::exception& ex) {
    return MakeUnexpected<ServiceIntrospection>(
        IntrospectionCodecErrorCode::kTypeSupportLoadFailed,
        std::format("Failed to load service type support for '{}': {}",
                    service_type, ex.what()));
  } catch (...) {
    return MakeUnexpected<ServiceIntrospection>(
        IntrospectionCodecErrorCode::kTypeSupportLoadFailed,
        std::format("Failed to load service type support for '{}'",
                    service_type));
  }

  if (service_type_support_intro == nullptr ||
      service_type_support_intro->typesupport_identifier == nullptr ||
      service_type_support_intro->data == nullptr ||
      std::string_view(service_type_support_intro->typesupport_identifier) !=
          rticpp::typesupport_identifier) [[unlikely]] {
    return MakeUnexpected<ServiceIntrospection>(
        IntrospectionCodecErrorCode::kInvalidTypeSupport,
        std::format(
            "Service '{}' does not expose introspection service type support",
            service_type));
  }

  const auto* service_members = static_cast<const IntrospectionServiceMembers*>(
      service_type_support_intro->data);
  if (service_members == nullptr ||
      service_members->request_members_ == nullptr ||
      service_members->response_members_ == nullptr) [[unlikely]] {
    return MakeUnexpected<ServiceIntrospection>(
        IntrospectionCodecErrorCode::kInvalidTypeSupport,
        std::format("Service '{}' has incomplete introspection metadata",
                    service_type));
  }

  if (output.service_typesupport == nullptr ||
      output.service_typesupport->request_typesupport == nullptr ||
      output.service_typesupport->response_typesupport == nullptr)
      [[unlikely]] {
    return MakeUnexpected<ServiceIntrospection>(
        IntrospectionCodecErrorCode::kInvalidTypeSupport,
        std::format("Service '{}' has incomplete cpp type support",
                    service_type));
  }

  request_message_typesupport_cpp =
      output.service_typesupport->request_typesupport;
  response_message_typesupport_cpp =
      output.service_typesupport->response_typesupport;

  request_message_typesupport_intro = ::get_message_typesupport_handle(
      request_message_typesupport_cpp, rticpp::typesupport_identifier);
  response_message_typesupport_intro = ::get_message_typesupport_handle(
      response_message_typesupport_cpp, rticpp::typesupport_identifier);

  const auto* request_members =
      ToIntrospectionMembers(request_message_typesupport_intro);
  const auto* response_members =
      ToIntrospectionMembers(response_message_typesupport_intro);
  if (request_members == nullptr || response_members == nullptr) [[unlikely]] {
    return MakeUnexpected<ServiceIntrospection>(
        IntrospectionCodecErrorCode::kInvalidTypeSupport,
        std::format("Service '{}' has invalid request/response introspection",
                    service_type));
  }

  output.request.typesupport_library = output.typesupport_library;
  output.request.message_typesupport = request_message_typesupport_cpp;
  output.request.message_members = request_members;

  output.response.typesupport_library = output.typesupport_library;
  output.response.message_typesupport = response_message_typesupport_cpp;
  output.response.message_members = response_members;

  return output;
}

auto ParseYamlToMessage(std::string_view yaml_text,
                        const MessageIntrospection& introspection,
                        void* message) -> IntrospectionCodecResult<void> {
  if (introspection.message_members == nullptr ||
      introspection.message_typesupport == nullptr) [[unlikely]] {
    return MakeUnexpected<void>(
        IntrospectionCodecErrorCode::kInvalidTypeSupport,
        "Message introspection is incomplete");
  }

  GenericYamlValue root;
  const auto read_error = glz::read_yaml(root, yaml_text);
  if (read_error) [[unlikely]] {
    return MakeUnexpected<void>(IntrospectionCodecErrorCode::kYamlParseFailed,
                                glz::format_error(read_error, yaml_text));
  }

  return ParseMessageValue(root, *introspection.message_members, message, "$");
}

auto FormatMessageAsJson(const MessageIntrospection& introspection,
                         const void* message)
    -> IntrospectionCodecResult<std::string> {
  if (introspection.message_members == nullptr ||
      introspection.message_typesupport == nullptr) [[unlikely]] {
    return MakeUnexpected<std::string>(
        IntrospectionCodecErrorCode::kInvalidTypeSupport,
        "Message introspection is incomplete");
  }

  std::string json;
  auto append_result =
      AppendMessageAsJson(*introspection.message_members, message, json, "$");
  if (!append_result) [[unlikely]] {
    return std::unexpected(append_result.error());
  }

  return json;
}

auto SerializeMessageToCdr(const MessageIntrospection& introspection,
                           const void* message)
    -> IntrospectionCodecResult<std::vector<uint8_t>> {
  if (introspection.message_members == nullptr ||
      introspection.message_typesupport == nullptr) [[unlikely]] {
    return MakeUnexpected<std::vector<uint8_t>>(
        IntrospectionCodecErrorCode::kInvalidTypeSupport,
        "Message introspection is incomplete");
  }

  try {
    rclcpp::SerializationBase serializer(introspection.message_typesupport);
    rclcpp::SerializedMessage serialized_message(
        introspection.message_members->size_of_);
    serializer.serialize_message(message, &serialized_message);

    const auto& rcl_serialized =
        serialized_message.get_rcl_serialized_message();
    std::vector<uint8_t> payload(rcl_serialized.buffer_length, 0);
    if (!payload.empty()) {
      std::memcpy(payload.data(), rcl_serialized.buffer,
                  rcl_serialized.buffer_length);
    }
    return payload;
  } catch (const std::exception& ex) {
    return MakeUnexpected<std::vector<uint8_t>>(
        IntrospectionCodecErrorCode::kSerializationFailed,
        std::format("Failed to serialize message: {}", ex.what()));
  } catch (...) {
    return MakeUnexpected<std::vector<uint8_t>>(
        IntrospectionCodecErrorCode::kSerializationFailed,
        "Failed to serialize message");
  }
}

auto DeserializeCdrToMessage(std::span<const uint8_t> payload,
                             const MessageIntrospection& introspection,
                             void* message) -> IntrospectionCodecResult<void> {
  if (introspection.message_members == nullptr ||
      introspection.message_typesupport == nullptr) [[unlikely]] {
    return MakeUnexpected<void>(
        IntrospectionCodecErrorCode::kInvalidTypeSupport,
        "Message introspection is incomplete");
  }

  try {
    rclcpp::SerializedMessage serialized_message(payload.size());
    auto& rcl_serialized = serialized_message.get_rcl_serialized_message();
    if (!payload.empty()) {
      std::memcpy(rcl_serialized.buffer, payload.data(), payload.size());
    }
    rcl_serialized.buffer_length = payload.size();

    rclcpp::SerializationBase serializer(introspection.message_typesupport);
    serializer.deserialize_message(&serialized_message, message);
    return {};
  } catch (const std::exception& ex) {
    return MakeUnexpected<void>(
        IntrospectionCodecErrorCode::kDeserializationFailed,
        std::format("Failed to deserialize CDR payload: {}", ex.what()));
  } catch (...) {
    return MakeUnexpected<void>(
        IntrospectionCodecErrorCode::kDeserializationFailed,
        "Failed to deserialize CDR payload");
  }
}

auto SerializeYamlToCdr(std::string_view yaml_text,
                        const MessageIntrospection& introspection)
    -> IntrospectionCodecResult<std::vector<uint8_t>> {
  auto message = DynamicMessage::Create(introspection);
  if (!message) [[unlikely]] {
    return std::unexpected(message.error());
  }

  auto parse_result =
      ParseYamlToMessage(yaml_text, introspection, message->Data());
  if (!parse_result) [[unlikely]] {
    return std::unexpected(parse_result.error());
  }

  return SerializeMessageToCdr(introspection, message->Data());
}

auto DeserializeCdrToYaml(std::span<const uint8_t> payload,
                          const MessageIntrospection& introspection)
    -> IntrospectionCodecResult<std::string> {
  auto message =
      DynamicMessage::Create(introspection, MessageInitialization::kZero);
  if (!message) [[unlikely]] {
    return std::unexpected(message.error());
  }

  auto deserialize_result =
      DeserializeCdrToMessage(payload, introspection, message->Data());
  if (!deserialize_result) [[unlikely]] {
    return std::unexpected(deserialize_result.error());
  }

  return FormatMessageAsJson(introspection, message->Data());
}

auto LoadDelayStampExtractor(std::string_view message_type)
    -> IntrospectionCodecResult<DelayStampExtractor> {
  auto message_introspection = LoadMessageIntrospection(message_type);
  if (!message_introspection) [[unlikely]] {
    return std::unexpected(message_introspection.error());
  }

  if (message_introspection->message_members == nullptr) [[unlikely]] {
    return MakeUnexpected<DelayStampExtractor>(
        IntrospectionCodecErrorCode::kInvalidTypeSupport,
        std::format("Message '{}' does not expose introspection members",
                    message_type));
  }

  DelayStampExtractor extractor{
      .message_introspection = std::move(*message_introspection),
      .offsets = {},
  };
  if (!ParseDelayStampOffsets(*extractor.message_introspection.message_members,
                              extractor.offsets)) [[unlikely]] {
    return MakeUnexpected<DelayStampExtractor>(
        IntrospectionCodecErrorCode::kUnsupportedFieldType,
        std::format("Message '{}' does not expose delay stamp layout (expects "
                    "header.stamp or builtin_interfaces/msg/Time fields)",
                    message_type));
  }

  return extractor;
}

auto ExtractStampSecondsFromCdr(std::span<const uint8_t> payload,
                                const DelayStampExtractor& extractor)
    -> std::optional<double> {
  const auto& introspection = extractor.message_introspection;
  const auto* message_members = introspection.message_members;
  if (message_members == nullptr ||
      introspection.message_typesupport == nullptr ||
      extractor.offsets.message_size == 0) [[unlikely]] {
    return std::nullopt;
  }

  if (message_members->init_function == nullptr ||
      message_members->fini_function == nullptr) [[unlikely]] {
    return std::nullopt;
  }

  const size_t time_base_offset =
      extractor.offsets.uses_header_field
          ? extractor.offsets.header_offset + extractor.offsets.stamp_offset
          : 0U;

  if (time_base_offset >= extractor.offsets.message_size) [[unlikely]] {
    return std::nullopt;
  }

  constexpr size_t kSecSize = sizeof(int32_t);
  constexpr size_t kNanosecSize = sizeof(uint32_t);
  if (extractor.offsets.sec_offset >
          extractor.offsets.message_size - time_base_offset ||
      extractor.offsets.nanosec_offset >
          extractor.offsets.message_size - time_base_offset) [[unlikely]] {
    return std::nullopt;
  }

  if (extractor.offsets.sec_offset + kSecSize >
          extractor.offsets.message_size - time_base_offset ||
      extractor.offsets.nanosec_offset + kNanosecSize >
          extractor.offsets.message_size - time_base_offset) [[unlikely]] {
    return std::nullopt;
  }

  auto message =
      DynamicMessage::Create(introspection, MessageInitialization::kAll);
  if (!message) [[unlikely]] {
    return std::nullopt;
  }

  if (!DeserializeCdrToMessage(payload, introspection, message->Data()))
      [[unlikely]] {
    return std::nullopt;
  }

  const auto* message_bytes =
      reinterpret_cast<const std::byte*>(message->Data());
  const std::byte* time_base = message_bytes + time_base_offset;

  int32_t sec = 0;
  uint32_t nanosec = 0;
  std::memcpy(&sec, time_base + extractor.offsets.sec_offset, sizeof(sec));
  std::memcpy(&nanosec, time_base + extractor.offsets.nanosec_offset,
              sizeof(nanosec));

  return ToStampSeconds(sec, nanosec);
}

}  // namespace roscraft::bridge::details
