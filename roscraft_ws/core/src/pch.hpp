#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <ios>
#include <iterator>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(__cpp_lib_print) && (__cpp_lib_print >= 202302L)
#include <print>
#endif

#ifdef ROSCRAFT_USE_STL_STACKTRACE
#include <stacktrace>
#else
#include <boost/stacktrace.hpp>
#endif
