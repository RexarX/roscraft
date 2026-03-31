#include <roscraft/bridge/common/backend.hpp>

#include <algorithm>
#include <array>

namespace roscraft::bridge::common {
namespace {

constexpr std::array<BackendType, 2> kBackends{
#if defined(ROSCRAFT_BRIDGE_ENABLE_JNI_BACKEND)
    BackendType::kJni,
#else
    BackendType::kNone,
#endif
#if defined(ROSCRAFT_BRIDGE_ENABLE_NETWORK_BACKEND)
    BackendType::kNetwork,
#else
    BackendType::kNone,
#endif
};

}  // namespace

auto AvailableBackends() noexcept -> const std::array<BackendType, 2>& {
  return kBackends;
}

auto SupportsBackend(const BackendType backend) noexcept -> bool {
  return std::find(kBackends.begin(), kBackends.end(), backend) !=
         kBackends.end();
}

}  // namespace roscraft::bridge::common
