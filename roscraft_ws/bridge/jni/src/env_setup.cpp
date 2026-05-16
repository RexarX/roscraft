#include <pch.hpp>

#include <roscraft/bridge/jni/env_setup.hpp>

#include <rclcpp/logging.hpp>
#include <rclcpp/rclcpp.hpp>

#include <dlfcn.h>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <string>

namespace {

std::filesystem::path DiscoverRos2BasePrefix() {
  Dl_info info{};
  if (dladdr(reinterpret_cast<const void*>(&rclcpp::init), &info) == 0 ||
      info.dli_fname == nullptr) {
    return {};
  }

  std::error_code ec;
  auto lib_path =
      std::filesystem::canonical(std::filesystem::path(info.dli_fname), ec);
  if (ec) [[unlikely]] {
    return {};
  }

  auto prefix = lib_path.parent_path();  // .../lib/librclcpp.so -> .../lib
  prefix = prefix.parent_path();         // .../lib -> .../ (ros2 base)
  return prefix;
}

void AddWorkspacePackages(const std::filesystem::path& install_base,
                          std::string& prefix_path) {
  std::error_code ec;
  for (const auto& entry :
       std::filesystem::directory_iterator(install_base, ec)) {
    if (ec) [[unlikely]] {
      break;
    }
    if (!entry.is_directory(ec)) {
      continue;
    }
    const auto share_dir = entry.path() / "share";
    if (!std::filesystem::is_directory(share_dir, ec)) {
      continue;
    }
    if (!prefix_path.empty()) {
      prefix_path += ':';
    }
    prefix_path += entry.path().string();
  }
}

bool TryDiscoverWorkspaceFromLibraryPath(std::string& prefix_path,
                                         const char* self_path) {
  if (self_path == nullptr) [[unlikely]] {
    return false;
  }

  std::error_code ec;
  auto lib_path =
      std::filesystem::canonical(std::filesystem::path(self_path), ec);
  if (ec) [[unlikely]] {
    return false;
  }

  // Library is at: <install_base>/<package>/lib/libroscraft_bridge_jni.so
  // Walk up: lib/ -> <package>/ -> <install_base>/
  auto pkg_prefix = lib_path.parent_path();  // .../lib
  pkg_prefix = pkg_prefix.parent_path();     // .../<package>

  // Verify this is actually a package directory (has share/)
  const auto share_dir = pkg_prefix / "share";
  if (!std::filesystem::is_directory(share_dir, ec)) {
    return false;
  }

  auto install_base = pkg_prefix.parent_path();  // .../<install_base>/

  AddWorkspacePackages(install_base, prefix_path);
  return true;
}

std::string BuildAmentPrefixPath(const char* self_lib_path) {
  // Check if AMENT_PREFIX_PATH is already set
  const char* existing = std::getenv("AMENT_PREFIX_PATH");
  if (existing != nullptr && existing[0] != '\0') {
    return {};
  }

  std::string prefix_path;

  // Discover ROS2 base prefix from librclcpp.so location
  const auto ros2_base = DiscoverRos2BasePrefix();
  if (!ros2_base.empty()) [[likely]] {
    prefix_path += ros2_base.string();
  } else {
    RCLCPP_WARN(rclcpp::get_logger("JniBridge"),
                "Failed to discover ROS2 base prefix; "
                "AMENT_PREFIX_PATH will not be set automatically.");
    return {};
  }

  // Try to discover workspace install base from library path
  TryDiscoverWorkspaceFromLibraryPath(prefix_path, self_lib_path);

  return prefix_path;
}

void SetupLdLibraryPath(const std::string& prefix_path) {
  if (prefix_path.empty()) [[unlikely]] {
    return;
  }

  const char* existing_ld = std::getenv("LD_LIBRARY_PATH");
  std::string new_ld;

  size_t pos = 0;
  size_t next = 0;
  while ((next = prefix_path.find(':', pos)) != std::string::npos) {
    const auto prefix = std::string_view(prefix_path.data() + pos, next - pos);
    const auto lib_dir = std::format("{}/lib", prefix);
    if (std::filesystem::is_directory(lib_dir)) {
      if (!new_ld.empty()) {
        new_ld += ':';
      }
      new_ld += lib_dir;
    }
    pos = next + 1;
  }

  const auto prefix = std::string_view(prefix_path.data() + pos);
  const auto last_lib = std::format("{}/lib", prefix);
  if (std::filesystem::is_directory(last_lib)) {
    if (!new_ld.empty()) {
      new_ld += ':';
    }
    new_ld += last_lib;
  }

  if (new_ld.empty()) [[unlikely]] {
    return;
  }

  if (existing_ld != nullptr && existing_ld[0] != '\0') {
    new_ld += ':';
    new_ld += existing_ld;
  }

  setenv("LD_LIBRARY_PATH", new_ld.c_str(), 1);
}

}  // namespace

namespace roscraft::bridge::jni {

void SetupRosEnvironment() {
  // Discover this shared library's own path using dladdr
  Dl_info info{};
  const char* self_path = nullptr;
  if (dladdr(reinterpret_cast<const void*>(&SetupRosEnvironment), &info) != 0) {
    self_path = info.dli_fname;
  }

  const auto prefix_path = BuildAmentPrefixPath(self_path);
  if (prefix_path.empty()) [[unlikely]] {
    return;
  }

  RCLCPP_INFO(rclcpp::get_logger("JniBridge"),
              "Auto-discovered AMENT_PREFIX_PATH: %s", prefix_path.c_str());

  setenv("AMENT_PREFIX_PATH", prefix_path.c_str(), 1);
  SetupLdLibraryPath(prefix_path);
}

}  // namespace roscraft::bridge::jni
