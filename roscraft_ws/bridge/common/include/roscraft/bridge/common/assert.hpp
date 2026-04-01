#pragma once

#include <roscraft/assert.hpp>

namespace roscraft::bridge::common {

/**
 * @brief Bridge assertion configuration.
 * @details When ROSCRAFT_BRIDGE_ENABLE_ROS2 is defined, assertion failures
 * are logged via the ROS2 logger (rclcpp). Otherwise, the default assertion
 * behavior from roscraft::assert.hpp is used.
 */
inline constexpr bool kEnableRos2AssertBridge =
#ifdef ROSCRAFT_BRIDGE_ENABLE_ROS2
    true;
#else
    false;
#endif

}  // namespace roscraft::bridge::common
