#pragma once

#include <roscraft/bridge/common/response/common.hpp>
#include <roscraft/bridge/common/response/header.hpp>
#include <roscraft/bridge/common/response/payload.hpp>

#include <cstddef>
#include <span>
#include <type_traits>

namespace roscraft::bridge::common {

/// @brief Complete response structure.
struct Response {
  ResponseHeader header;
  ResponsePayload payload;

  /// @brief Construct success response.
  /// @param response_id Response ID to match
  /// @return Response instance
  [[nodiscard]] constexpr Response From(ResponseId response_id) noexcept {
    Response response;
    response.header.response_id = response_id;
    response.header.status = ResponseStatus::kSuccess;
    response.header.error = ResponseError::kNone;
    return response;
  }

  /// @brief Construct error response.
  /// @param response_id Response ID to match
  /// @param err Error code
  /// @return Response instance
  [[nodiscard]] constexpr Response From(ResponseId response_id,
                                        ResponseError err) noexcept {
    Response response;
    response.header.response_id = response_id;
    response.header.status = ResponseStatus::kError;
    response.header.error = err;
    return response;
  }

  /// @brief Construct success response with payload.
  /// @param response_id Response ID to match
  /// @param payload_data Payload data
  /// @return Response instance
  [[nodiscard]] constexpr Response From(
      ResponseId response_id,
      std::span<const std::byte> payload_data) noexcept {
    Response response;
    response.header.response_id = response_id;
    response.header.status = ResponseStatus::kSuccess;
    response.header.error = ResponseError::kNone;
    response.payload.SetInline(payload_data);
    response.header.payload_size = response.payload.size;
    return response;
  }

  /// @brief Check if response indicates success.
  /// @return `true` if status is `kSuccess`, `false` otherwise
  [[nodiscard]] constexpr bool Success() const noexcept {
    return header.status == ResponseStatus::kSuccess;
  }

  /// @brief Check if response indicates error.
  /// @return `true` if status is `kError`, `false` otherwise
  [[nodiscard]] constexpr bool Failed() const noexcept {
    return header.status == ResponseStatus::kError;
  }

  /// @brief Check if response is pending.
  /// @return `true` if status is `kPending`, `false` otherwise
  [[nodiscard]] constexpr bool Pending() const noexcept {
    return header.status == ResponseStatus::kPending;
  }

  /// @brief Get mutable payload span.
  /// @return Payload data as a `std::span<std::byte>`
  [[nodiscard]] constexpr auto Payload() noexcept -> std::span<std::byte> {
    return payload.Data();
  }

  /// @brief Get read-only payload span.
  /// @return Payload data as a `std::span<const std::byte>`
  [[nodiscard]] constexpr auto Payload() const noexcept
      -> std::span<const std::byte> {
    return payload.Data();
  }
};

static_assert(std::is_trivially_copyable_v<Response>,
              "Response should be trivially copyable");

}  // namespace roscraft::bridge::common
