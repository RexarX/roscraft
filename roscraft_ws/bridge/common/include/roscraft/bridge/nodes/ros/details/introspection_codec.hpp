#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct rosidl_message_type_support_t;
struct rosidl_service_type_support_t;

namespace rcpputils {

class SharedLibrary;

}

namespace rosidl_typesupport_introspection_cpp {

struct MessageMember_s;
struct MessageMembers_s;

}  // namespace rosidl_typesupport_introspection_cpp

namespace rticpp = rosidl_typesupport_introspection_cpp;

namespace roscraft::bridge::details {

enum class MessageInitialization : uint8_t {
  kAll,
  kZero,
  kDefaults,
  kSkip,
};

enum class IntrospectionCodecErrorCode : uint8_t {
  kInvalidTypeSupport = 0,
  kTypeSupportLoadFailed,
  kYamlParseFailed,
  kInvalidYamlShape,
  kUnknownField,
  kUnsupportedFieldType,
  kInvalidFieldValue,
  kArraySizeMismatch,
  kSerializationFailed,
  kDeserializationFailed,
};

struct IntrospectionCodecError {
  IntrospectionCodecErrorCode code =
      IntrospectionCodecErrorCode::kInvalidTypeSupport;
  std::string message;

  [[nodiscard]] static IntrospectionCodecError From(
      IntrospectionCodecErrorCode code, std::string message) {
    return {code, std::move(message)};
  }
};

template <typename ValueT>
using IntrospectionCodecResult = std::expected<ValueT, IntrospectionCodecError>;

struct MessageIntrospection {
  std::shared_ptr<rcpputils::SharedLibrary> typesupport_library;
  const rosidl_message_type_support_t* message_typesupport = nullptr;
  const rticpp::MessageMembers_s* message_members = nullptr;
};

struct ServiceIntrospection {
  std::shared_ptr<rcpputils::SharedLibrary> typesupport_library;
  const rosidl_service_type_support_t* service_typesupport = nullptr;
  MessageIntrospection request;
  MessageIntrospection response;
};

/// @brief Byte offsets for extracting delay stamp seconds from a message.
struct DelayStampOffsets {
  size_t message_size = 0;
  bool uses_header_field = false;
  size_t header_offset = 0;
  size_t stamp_offset = 0;
  size_t sec_offset = 0;
  size_t nanosec_offset = 0;
};

/// @brief Cached introspection and offsets for delay stamp extraction.
struct DelayStampExtractor {
  MessageIntrospection message_introspection;
  DelayStampOffsets offsets;
};

class DynamicMessage {
public:
  DynamicMessage() = delete;
  DynamicMessage(const DynamicMessage&) = delete;
  DynamicMessage(DynamicMessage&& other) noexcept;
  ~DynamicMessage();

  DynamicMessage& operator=(const DynamicMessage&) = delete;
  DynamicMessage& operator=(DynamicMessage&& other) noexcept;

  [[nodiscard]] static auto Create(
      const MessageIntrospection& introspection,
      MessageInitialization initialization = MessageInitialization::kAll)
      -> IntrospectionCodecResult<DynamicMessage>;

  [[nodiscard]] void* Data() noexcept { return storage_.data(); }
  [[nodiscard]] const void* Data() const noexcept { return storage_.data(); }
  [[nodiscard]] size_t Size() const noexcept { return storage_.size(); }

private:
  explicit DynamicMessage(const rticpp::MessageMembers_s* message_members,
                          std::vector<uint8_t>&& storage);

  void Finalize();

  const rticpp::MessageMembers_s* message_members_ = nullptr;
  std::vector<uint8_t> storage_;
};

inline DynamicMessage::DynamicMessage(
    const rticpp::MessageMembers_s* message_members,
    std::vector<uint8_t>&& storage)
    : message_members_(message_members), storage_(std::move(storage)) {}

inline DynamicMessage::DynamicMessage(DynamicMessage&& other) noexcept
    : message_members_(other.message_members_),
      storage_(std::move(other.storage_)) {
  other.message_members_ = nullptr;
}

inline DynamicMessage::~DynamicMessage() {
  Finalize();
}

inline DynamicMessage& DynamicMessage::operator=(
    DynamicMessage&& other) noexcept {
  if (this == &other) [[unlikely]] {
    return *this;
  }

  Finalize();
  message_members_ = other.message_members_;
  storage_ = std::move(other.storage_);
  other.message_members_ = nullptr;
  return *this;
}

/// @brief Converts a message type support handle to introspection members.
/// @param type_support The message type support handle to convert.
/// @return Pointer to introspection message members, or `nullptr` if invalid.
/// @warning Triggers assertion if `type_support` is invalid but non-null.
[[nodiscard]] auto ToIntrospectionMembers(
    const rosidl_message_type_support_t* type_support)
    -> const rticpp::MessageMembers_s*;

/// @brief Finds a member by name in the introspection message members.
/// @param members The introspection message members to search.
/// @param name The member name to find.
/// @return Pointer to the found member, or `nullptr` if not found.
[[nodiscard]] auto FindMemberByName(const rticpp::MessageMembers_s& members,
                                    std::string_view name)
    -> const rticpp::MessageMember_s*;

/// @brief Finds a member offset by name in the introspection message members.
/// @param members The introspection message members to search.
/// @param name The member name to find.
/// @return Offset of the found member, or `std::nullopt` if not found.
[[nodiscard]] auto FindMemberOffset(const rticpp::MessageMembers_s& members,
                                    std::string_view name)
    -> std::optional<size_t>;

[[nodiscard]] auto LoadMessageIntrospection(std::string_view message_type)
    -> IntrospectionCodecResult<MessageIntrospection>;

[[nodiscard]] auto LoadServiceIntrospection(std::string_view service_type)
    -> IntrospectionCodecResult<ServiceIntrospection>;

[[nodiscard]] auto ParseYamlToMessage(std::string_view yaml_text,
                                      const MessageIntrospection& introspection,
                                      void* message)
    -> IntrospectionCodecResult<void>;

[[nodiscard]] auto FormatMessageAsJson(
    const MessageIntrospection& introspection, const void* message)
    -> IntrospectionCodecResult<std::string>;

[[nodiscard]] auto SerializeMessageToCdr(
    const MessageIntrospection& introspection, const void* message)
    -> IntrospectionCodecResult<std::vector<uint8_t>>;

[[nodiscard]] auto DeserializeCdrToMessage(
    std::span<const uint8_t> payload, const MessageIntrospection& introspection,
    void* message) -> IntrospectionCodecResult<void>;

[[nodiscard]] auto SerializeYamlToCdr(std::string_view yaml_text,
                                      const MessageIntrospection& introspection)
    -> IntrospectionCodecResult<std::vector<uint8_t>>;

[[nodiscard]] auto DeserializeCdrToYaml(
    std::span<const uint8_t> payload, const MessageIntrospection& introspection)
    -> IntrospectionCodecResult<std::string>;

/// @brief Loads delay stamp extractor metadata for a message type.
/// @param message_type Fully-qualified ROS message type.
/// @return Extractor metadata or an introspection error.
[[nodiscard]] auto LoadDelayStampExtractor(std::string_view message_type)
    -> IntrospectionCodecResult<DelayStampExtractor>;

/// @brief Extracts stamp seconds from serialized CDR payload.
/// @param payload Serialized message payload in CDR format.
/// @param extractor Delay stamp extraction metadata.
/// @return Extracted stamp seconds or `std::nullopt` on decode/extract failure.
[[nodiscard]] auto ExtractStampSecondsFromCdr(
    std::span<const uint8_t> payload, const DelayStampExtractor& extractor)
    -> std::optional<double>;

}  // namespace roscraft::bridge::details
