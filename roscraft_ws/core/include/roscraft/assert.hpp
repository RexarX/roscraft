#pragma once

#include <roscraft/compiler.hpp>
#include <roscraft/platform.hpp>
#include <roscraft/utils/macro.hpp>

#include <cstdio>
#include <cstdlib>
#include <format>
#include <functional>
#include <source_location>
#include <string>
#include <string_view>

#if defined(__cpp_lib_print) && (__cpp_lib_print >= 202302L)
#include <print>
#endif

namespace roscraft {

/**
 * @brief Function signature for custom assertion handlers.
 * @param condition The failed condition as a string
 * @param loc Source location of the assertion
 * @param message Additional message (may be empty)
 */
using AssertionHandler = void (*)(std::string_view condition,
                                  const std::source_location& loc,
                                  std::string_view message) noexcept;

/// @brief Default assertion handler (`nullptr` means use built-in default
/// behavior).
inline constexpr AssertionHandler kDefaultAssertionHandler = nullptr;

namespace details {

#ifdef ROSCRAFT_ENABLE_ASSERTS
inline constexpr bool kEnableAssert = true;
#else
inline constexpr bool kEnableAssert = false;
#endif

/**
 * @brief Prints the current stack trace to stderr.
 * @details No-op when stacktrace support is disabled.
 * @param loc Source location used to select the first relevant frame
 */
void PrintCurrentStackTrace(const std::source_location& loc) noexcept;

/**
 * @brief Storage for the custom assertion handler.
 * @details When set, this handler is called instead of the default behavior.
 * The handler can be set by calling `SetAssertionHandler()`.
 */
inline AssertionHandler g_custom_assertion_handler = nullptr;

/**
 * @brief Default assertion handler that prints to stderr.
 * @param condition The failed condition as a string
 * @param loc Source location of the assertion
 * @param message Additional message
 */
inline void DefaultAssertionHandler(std::string_view condition,
                                    const std::source_location& loc,
                                    std::string_view message) noexcept {
#if defined(__cpp_lib_print) && (__cpp_lib_print >= 202302L)
  if (!message.empty()) {
    std::println(stderr, "Assertion failed: {} | {}", condition, message);
    std::println(stderr, "  File: {}:{}", loc.file_name(), loc.line());
    std::println(stderr, "  Function: {}", loc.function_name());
  } else {
    std::println(stderr, "Assertion failed: {}", condition);
    std::println(stderr, "  File: {}:{}", loc.file_name(), loc.line());
    std::println(stderr, "  Function: {}", loc.function_name());
  }
#else
  if (!message.empty()) {
    std::fprintf(stderr, "Assertion failed: %.*s | %.*s\n",
                 static_cast<int>(condition.size()), condition.data(),
                 static_cast<int>(message.size()), message.data());
    std::fprintf(stderr, "  File: %s:%u\n", loc.file_name(), loc.line());
    std::fprintf(stderr, "  Function: %s\n", loc.function_name());
  } else {
    std::fprintf(stderr, "Assertion failed: %.*s\n",
                 static_cast<int>(condition.size()), condition.data());
    std::fprintf(stderr, "  File: %s:%u\n", loc.file_name(), loc.line());
    std::fprintf(stderr, "  Function: %s\n", loc.function_name());
  }
#endif
  std::fflush(stderr);

#ifdef ROSCRAFT_ENABLE_STACKTRACE
  PrintCurrentStackTrace(loc);
#endif
}

/**
 * @brief Logger assertion handler (weak symbol).
 * @details This is defined as a weak symbol that defaults to `nullptr`.
 * The implementation should log via the logger with critical level.
 * @param condition The failed condition as a string
 * @param loc Source location of the assertion
 * @param message Additional message
 */
#ifdef _MSC_VER
// MSVC doesn't support weak symbols, so we use a different approach.
// The logger will set this via `SetLoggerHandler` at initialization.
inline AssertionHandler g_loger_handler = nullptr;

inline void LoggerAssertionHandler(std::string_view condition,
                                   const std::source_location& loc,
                                   std::string_view message) noexcept {
  if (g_loger_handler) {
    g_loger_handler(condition, loc, message);
  }
}

inline bool HasLoggerHandler() noexcept {
  return g_loger_handler != nullptr;
}

/**
 * @brief Sets the logger handler (called by logger at initialization).
 * @param handler The handler function from the logger
 */
inline void SetLoggerHandler(AssertionHandler handler) noexcept {
  g_loger_handler = handler;
}

#else
// GCC/Clang support weak symbols

/**
 * @brief Weak symbol for logger handler check.
 * @details The logger provides the real implementation.
 * @return true if the logger handler is available
 */
[[gnu::weak]] bool HasLoggerHandler() noexcept;

/**
 * @brief Weak symbol for logger assertion handler.
 * @details The logger provides the real implementation.
 */
[[gnu::weak]] void LoggerAssertionHandler(std::string_view condition,
                                          const std::source_location& loc,
                                          std::string_view message) noexcept;

#endif

/**
 * @brief Unified assertion handling function.
 * @details Priority order:
 *   1. Custom handler (if set by user via SetAssertionHandler)
 *   2. Log handler (if logger is available and linked)
 *   3. Default handler (prints to stderr)
 *
 * @param condition The failed condition as a string
 * @param loc Source location of the assertion
 * @param message Additional message
 */
inline void HandleAssertion(std::string_view condition,
                            const std::source_location& loc,
                            std::string_view message) noexcept {
  // Priority 1: Custom user handler
  if (g_custom_assertion_handler != nullptr) {
    g_custom_assertion_handler(condition, loc, message);
    return;
  }

  // Priority 2: Logger handler (if available)
  if (HasLoggerHandler != nullptr && LoggerAssertionHandler != nullptr &&
      HasLoggerHandler()) {
    LoggerAssertionHandler(condition, loc, message);
    return;
  }

  // Priority 3: Default handler (printf/println to stderr)
  DefaultAssertionHandler(condition, loc, message);
}

}  // namespace details

/**
 * @brief Sets a custom assertion handler.
 * @details When set, this handler is called for all assertion failures instead
 * of the default behavior. Set to `nullptr` to restore default behavior.
 *
 * Priority order for assertion handling:
 *   1. Custom handler (if set via this function)
 *   2. Log handler (if logger is linked)
 *   3. Default handler (prints to stderr)
 *
 * @param handler The custom handler function, or `nullptr` to use default
 *
 * @example
 * @code
 * // Set custom handler
 * roscraft::SetAssertionHandler([](std::string_view condition, const
 * std::source_location& loc, std::string_view message) noexcept {
 *   // Your custom logging here
 *   MyLogger::Critical("Assert: {} | {} [{}:{}]", condition, message,
 * loc.file_name(), loc.line());
 * });
 *
 * // Reset to default behavior
 * roscraft::SetAssertionHandler(roscraft::kDefaultAssertionHandler);
 * @endcode
 */
inline void SetAssertionHandler(AssertionHandler handler) noexcept {
  details::g_custom_assertion_handler = handler;
}

/**
 * @brief Gets the current custom assertion handler.
 * @return The current custom handler, or `nullptr` if using default behavior
 */
[[nodiscard]] inline AssertionHandler GetAssertionHandler() noexcept {
  return details::g_custom_assertion_handler;
}

/**
 * @brief Prints a message with stack trace and aborts the program execution.
 * @details Useful for placing in dead code branches or unreachable states.
 * @param message The message to print before aborting
 */
void AbortWithStacktrace(std::string_view message) noexcept;

}  // namespace roscraft

// NOLINTBEGIN(cppcoreguidelines-avoid-do-while)
// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/**
 * @brief Assertion macro that aborts execution in debug builds.
 * @details Does nothing in release builds.
 * Uses the configured assertion handler (custom -> logger -> default).
 * Supports format strings and arguments.
 * @param condition The condition to check
 * @param ... Optional message (can be format string with arguments)
 * @hideinitializer
 */
#ifdef ROSCRAFT_ENABLE_ASSERTS
#define ROSCRAFT_ASSERT(condition, ...)                                 \
  do {                                                                  \
    if constexpr (::roscraft::details::kEnableAssert) {                 \
      if (ROSCRAFT_EXPECT_FALSE(!(condition))) [[unlikely]] {           \
        constexpr auto loc = std::source_location::current();           \
        if constexpr (sizeof(#__VA_ARGS__) > 1) {                       \
          try {                                                         \
            const std::string msg = std::format("" __VA_ARGS__);        \
            ::roscraft::details::HandleAssertion(#condition, loc, msg); \
          } catch (...) {                                               \
            ::roscraft::details::HandleAssertion(                       \
                #condition, loc, "Formatting error in assertion");      \
          }                                                             \
        } else {                                                        \
          ::roscraft::details::HandleAssertion(#condition, loc, "");    \
        }                                                               \
        ROSCRAFT_DEBUG_BREAK();                                         \
      }                                                                 \
    }                                                                   \
  } while (false)
#else
#define ROSCRAFT_ASSERT(condition, ...)                          \
  [[maybe_unused]] static constexpr auto ROSCRAFT_ANONYMOUS_VAR( \
      unused_assert) = 0
#endif

/**
 * @brief Invariant check that asserts in debug builds and logs error in
 * release.
 * @details Provides runtime safety checks that are enforced even in release
 * builds. In debug builds, triggers assertion. In release builds, logs error
 * and continues.
 * @param condition The condition to check
 * @param ... Optional message (can be format string with arguments)
 * @hideinitializer
 */
#ifdef ROSCRAFT_ENABLE_ASSERTS
#define ROSCRAFT_INVARIANT(condition, ...)                            \
  do {                                                                \
    if (ROSCRAFT_EXPECT_FALSE(!(condition))) [[unlikely]] {           \
      constexpr auto loc = std::source_location::current();           \
      if constexpr (sizeof(#__VA_ARGS__) > 1) {                       \
        try {                                                         \
          const std::string msg = std::format("" __VA_ARGS__);        \
          ::roscraft::details::HandleAssertion(#condition, loc, msg); \
        } catch (...) {                                               \
          ::roscraft::details::HandleAssertion(                       \
              #condition, loc, "Formatting error in invariant");      \
        }                                                             \
      } else {                                                        \
        ::roscraft::details::HandleAssertion(#condition, loc, "");    \
      }                                                               \
      ROSCRAFT_DEBUG_BREAK();                                         \
    }                                                                 \
  } while (false)
#else
#define ROSCRAFT_INVARIANT(condition, ...)                            \
  do {                                                                \
    if (ROSCRAFT_EXPECT_FALSE(!(condition))) [[unlikely]] {           \
      constexpr auto loc = std::source_location::current();           \
      if constexpr (sizeof(#__VA_ARGS__) > 1) {                       \
        try {                                                         \
          const std::string msg = std::format("" __VA_ARGS__);        \
          ::roscraft::details::HandleAssertion(#condition, loc, msg); \
        } catch (...) {                                               \
          ::roscraft::details::HandleAssertion(#condition, loc, "");  \
        }                                                             \
      } else {                                                        \
        ::roscraft::details::HandleAssertion(#condition, loc, "");    \
      }                                                               \
    }                                                                 \
  } while (false)
#endif

/**
 * @brief Verify macro that always checks the condition.
 * @details Similar to assert but runs in both debug and release builds.
 * Useful for validating external input or critical invariants.
 * @param condition The condition to check
 * @param ... Optional message (can be format string with arguments)
 * @hideinitializer
 */
#define ROSCRAFT_VERIFY(condition, ...)                                       \
  do {                                                                        \
    if (ROSCRAFT_EXPECT_FALSE(!(condition))) [[unlikely]] {                   \
      constexpr auto loc = std::source_location::current();                   \
      if constexpr (sizeof(#__VA_ARGS__) > 1) {                               \
        try {                                                                 \
          const std::string msg = std::format("" __VA_ARGS__);                \
          ::roscraft::details::HandleAssertion(#condition, loc, msg);         \
        } catch (...) {                                                       \
          ::roscraft::details::HandleAssertion(#condition, loc,               \
                                               "Formatting error in verify"); \
        }                                                                     \
      } else {                                                                \
        ::roscraft::details::HandleAssertion(#condition, loc, "");            \
      }                                                                       \
      ROSCRAFT_DEBUG_BREAK();                                                 \
    }                                                                         \
  } while (false)

// NOLINTEND(cppcoreguidelines-macro-usage)
// NOLINTEND(cppcoreguidelines-avoid-do-while)
