#include <pch.hpp>

#include <roscraft/assert.hpp>
#include <roscraft/stacktrace.hpp>

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>
#include <string_view>

#if defined(__cpp_lib_print) && (__cpp_lib_print >= 202302L)
#include <print>
#endif

namespace {

constexpr size_t kDefaultAssertionStacktraceFrames = 10;

[[nodiscard]] roscraft::StacktraceConfig BuildAssertionStacktraceConfig(
    const std::source_location& loc) noexcept {
  auto config = roscraft::StacktraceConfig::FromSourceLocation(loc);
  config.start_frame = 1;
  config.max_frames = kDefaultAssertionStacktraceFrames;
  config.stop_before = "__libc_start_main";
  return config;
}

}  // namespace

namespace roscraft {

namespace details {

void PrintCurrentStackTrace(const std::source_location& loc) noexcept {
#ifdef ROSCRAFT_ENABLE_STACKTRACE
  const auto stacktrace =
      Stacktrace::Capture(BuildAssertionStacktraceConfig(loc));
  const std::string text = stacktrace.ToString();

#if defined(__cpp_lib_print) && (__cpp_lib_print >= 202302L)
  std::println(stderr, "\n{}", text);
#else
  std::fprintf(stderr, "\n%s\n", text.c_str());
#endif
#else
  (void)loc;
#endif
}

}  // namespace details

void AbortWithStacktrace(std::string_view message) noexcept {
#if defined(__cpp_lib_print) && (__cpp_lib_print >= 202302L)
  std::println(stderr, "\n=== FATAL ERROR ===");
  std::println(stderr, "Message: {}", message);

#ifdef ROSCRAFT_ENABLE_STACKTRACE
  const auto stacktrace = Stacktrace::Capture(
      BuildAssertionStacktraceConfig(std::source_location::current()));
  std::println(stderr, "\n{}", stacktrace.ToString());
#else
  std::println(
      stderr,
      "\nStack trace: <not available - build with ROSCRAFT_ENABLE_STACKTRACE>");
#endif

  std::println(stderr, "===================\n");
#else
  std::fprintf(stderr, "\n=== FATAL ERROR ===\n");
  std::fprintf(stderr, "Message: %.*s\n", static_cast<int>(message.size()),
               message.data());

#ifdef ROSCRAFT_ENABLE_STACKTRACE
  const auto stacktrace = Stacktrace::Capture(
      BuildAssertionStacktraceConfig(std::source_location::current()));
  const std::string text = stacktrace.ToString();
  std::fprintf(stderr, "\n%s\n", text.c_str());
#else
  std::fprintf(stderr,
               "\nStack trace: <not available - build with "
               "ROSCRAFT_ENABLE_STACKTRACE>\n");
#endif

  std::fprintf(stderr, "===================\n\n");
#endif
  std::fflush(stderr);

  ROSCRAFT_DEBUG_BREAK();
  std::abort();
}

namespace details {

#ifndef _MSC_VER

#if defined(__GNUC__) || defined(__clang__)
[[gnu::weak]]
#endif
bool HasLoggerHandler() noexcept {
  return false;
}

#if defined(__GNUC__) || defined(__clang__)
[[gnu::weak]]
#endif
void LoggerAssertionHandler(
    [[maybe_unused]] std::string_view condition,
    [[maybe_unused]] const std::source_location& loc,
    [[maybe_unused]] std::string_view message) noexcept {
}

#endif  // !_MSC_VER

}  // namespace details

}  // namespace roscraft
