#pragma once

#include <roscraft/bridge/common/response/common.hpp>

#include <cstdint>
#include <type_traits>

namespace roscraft::bridge::common {

/// @brief Response header (fixed-size, cache-friendly).
struct ResponseHeader {
  ResponseId response_id = 0;                        ///< Matching resource ID
  ResponseStatus status = ResponseStatus::kSuccess;  ///< Response status

  /// Error code (if status is error)
  ResponseError error = ResponseError::kNone;

  uint16_t flags = 0;         ///< Additional flags
  uint32_t payload_size = 0;  ///< Payload size in bytes
};

static_assert(sizeof(ResponseHeader) <= 24,
              "ResponseHeader should fit in 1/3 of a cache line");
static_assert(std::is_trivially_copyable_v<ResponseHeader>,
              "ResponseHeader should be trivially copyable");

}  // namespace roscraft::bridge::common
