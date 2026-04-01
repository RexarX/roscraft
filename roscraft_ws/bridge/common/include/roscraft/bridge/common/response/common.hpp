#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

namespace roscraft::bridge::common {

/// @brief Response identifier type.
using ResponseId = uint64_t;

/// @brief Response status codes.
enum class ResponseStatus : uint8_t {
  kSuccess = 0,
  kError = 1,
  kPending = 2,
  kTimeout = 3,
  kCancelled = 4,
  kNotFound = 5,
  kInvalidRequest = 6,
  kPermissionDenied = 7,
  kInternalError = 8,
};

/// @brief Get human-readable name for ResponseStatus.
/// @param status Status to convert
/// @return Human-readable status name
[[nodiscard]] constexpr std::string_view ToString(
    ResponseStatus status) noexcept {
  switch (status) {
    case ResponseStatus::kSuccess:
      return "Success";
    case ResponseStatus::kError:
      return "Error";
    case ResponseStatus::kPending:
      return "Pending";
    case ResponseStatus::kTimeout:
      return "Timeout";
    case ResponseStatus::kCancelled:
      return "Cancelled";
    case ResponseStatus::kNotFound:
      return "NotFound";
    case ResponseStatus::kInvalidRequest:
      return "InvalidRequest";
    case ResponseStatus::kPermissionDenied:
      return "PermissionDenied";
    case ResponseStatus::kInternalError:
      return "InternalError";
  }
  return "Unknown";
}

/// @brief Response error codes.
enum class ResponseError : uint8_t {
  kNone = 0,
  kInvalidKind = 1,
  kInvalidPayload = 2,
  kQueueFull = 3,
  kBackendNotInitialized = 4,
  kBackendAlreadyInitialized = 5,
  kTimeout = 6,
  kInternalError = 7,
  kPermissionDenied = 8,
  kNotFound = 9,
  kSerializationError = 10,
  kDeserializationError = 11,
  kConnectionError = 12,
  kJniError = 13,
  kNetworkError = 14,
};

/// @brief Get human-readable name for `ResponseError`.
/// @param error Error to convert
/// @return Human-readable error name
[[nodiscard]] constexpr std::string_view ToString(
    ResponseError error) noexcept {
  switch (error) {
    case ResponseError::kNone:
      return "None";
    case ResponseError::kInvalidKind:
      return "InvalidKind";
    case ResponseError::kInvalidPayload:
      return "InvalidPayload";
    case ResponseError::kQueueFull:
      return "QueueFull";
    case ResponseError::kBackendNotInitialized:
      return "BackendNotInitialized";
    case ResponseError::kBackendAlreadyInitialized:
      return "BackendAlreadyInitialized";
    case ResponseError::kTimeout:
      return "Timeout";
    case ResponseError::kInternalError:
      return "InternalError";
    case ResponseError::kPermissionDenied:
      return "PermissionDenied";
    case ResponseError::kNotFound:
      return "NotFound";
    case ResponseError::kSerializationError:
      return "SerializationError";
    case ResponseError::kDeserializationError:
      return "DeserializationError";
    case ResponseError::kConnectionError:
      return "ConnectionError";
    case ResponseError::kJniError:
      return "JniError";
    case ResponseError::kNetworkError:
      return "NetworkError";
  }
  return "Unknown";
}

/// @brief Result type for responses.
template <typename T>
using ResponseResult = std::expected<T, ResponseError>;

}  // namespace roscraft::bridge::common
