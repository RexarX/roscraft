#pragma once

#include <roscraft/bridge/bridge.hpp>

#include <concepts>
#include <memory>

namespace roscraft::bridge {

/// @brief Configuration for the `App`.
struct AppConfig {
  std::unique_ptr<Bridge> bridge;  ///< Bridge used by the `App`

  int argc = 0;           ///< Number of command line arguments
  char** argv = nullptr;  ///< Command line arguments

  /// @brief Returns command-line argument count.
  [[nodiscard]] constexpr int Argc() const noexcept { return argc; }

  /// @brief Returns command-line argument vector.
  [[nodiscard]] constexpr char** Argv() const noexcept { return argv; }

  /// @brief Creates an `AppConfig` from a bridge
  /// @param bridge The bridge to use
  /// @return The `AppConfig` created from the bridge
  [[nodiscard]] static constexpr AppConfig From(
      std::unique_ptr<Bridge> bridge) {
    AppConfig config;
    config.bridge = std::move(bridge);
    return config;
  }

  /// @brief Creates an `AppConfig` from a bridge trait and arguments
  /// @tparam T The bridge trait to use
  /// @tparam Args The types of the arguments to pass to the bridge trait
  /// @param args The arguments to pass to the bridge trait
  /// @return The `AppConfig` created from the bridge trait and arguments
  template <BridgeTrait T, typename... Args>
    requires std::constructible_from<T, Args...>
  [[nodiscard]] static constexpr AppConfig From(Args&&... args) {
    return From(std::make_unique<T>(std::forward<Args>(args)...));
  }

  /// @brief Creates an `AppConfig` from command line arguments
  /// @param argc Number of command line arguments
  /// @param argv Command line arguments
  /// @return The `AppConfig` created from the command line arguments
  [[nodiscard]] static constexpr AppConfig From(int argc, char* argv[]) {
    AppConfig config;
    config.argc = argc;
    config.argv = argv;
    return config;
  }

  /// @brief Creates an `AppConfig` from a bridge and command line arguments
  /// @param bridge The bridge to use
  /// @param argc Number of command line arguments
  /// @param argv Command line arguments
  /// @return The `AppConfig` created from the bridge and command line arguments
  [[nodiscard]] static constexpr AppConfig From(std::unique_ptr<Bridge> bridge,
                                                int argc, char* argv[]) {
    AppConfig config;
    config.argc = argc;
    config.argv = argv;
    config.bridge = std::move(bridge);
    return config;
  }

  template <BridgeTrait T, typename... Args>
    requires std::constructible_from<T, Args...>
  [[nodiscard]] static constexpr AppConfig From(int argc, char* argv[],
                                                Args&&... args) {
    return From(std::make_unique<T>(std::forward<Args>(args)...), argc, argv);
  }

  /// @brief Checks if the configuration is valid.
  /// @return `true` if the configuration is valid, `false` otherwise
  [[nodiscard]] constexpr bool Valid() const noexcept {
    return bridge != nullptr;
  }

  /// @brief Checks if the configuration has command line arguments.
  /// @return `true` if the configuration has command line arguments, `false`
  /// otherwise
  [[nodiscard]] constexpr bool HasCommandLineArgs() const noexcept {
    return argc > 0 && argv != nullptr;
  }
};

}  // namespace roscraft::bridge
