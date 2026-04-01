#pragma once

namespace roscraft::bridge::common {

/// @brief Abstract base class for bridge implementations.
class Bridge {
public:
  virtual ~Bridge() = default;
};

}  // namespace roscraft::bridge::common
