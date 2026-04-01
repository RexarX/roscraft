# Asio dependency configuration (standalone, not Boost.Asio)
#
# This module handles finding standalone Asio from multiple sources:
# 1. System packages (pacman, apt, etc.)
# 2. CPM download (fallback)
#
# Standalone Asio provides:
# - Native C++20/23 coroutines support
# - No Boost dependency overhead
# - Header-only option available
# - Modern asio:: namespace
#
# Usage:
#   roscraft_require_dependency(Asio)

include_guard(GLOBAL)

# Check if already processed
roscraft_dep_is_processed(NAME "Asio" OUTPUT_VAR _asio_processed)
if(_asio_processed)
    return()
endif()

roscraft_dep_header(NAME "Asio")

# Use roscraft_module system for standard package finding
roscraft_dep_begin(
    NAME asio
    VERSION 1.28
    DEBIAN_NAMES libasio-dev
    RPM_NAMES asio-devel
    PACMAN_NAMES asio
    BREW_NAMES asio
    PKG_CONFIG_NAMES asio
    CPM_NAME asio
    CPM_VERSION 1.38.0
    CPM_GITHUB_REPOSITORY chriskohlhoff/asio
    CPM_OPTIONS
        "ASIO_STANDALONE ON"
        "ASIO_NO_DEPRECATED ON"
)
roscraft_dep_end()

# Define compile definitions to ensure standalone Asio is used
if(TARGET asio)
    target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)

    # Platform-specific system libraries
    if(WIN32)
        target_link_libraries(asio INTERFACE ws2_32 mswsock)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(asio INTERFACE pthread)
    endif()
endif()

# Create roscraft::asio alias target
if(TARGET asio AND NOT TARGET roscraft::asio)
    add_library(roscraft::asio ALIAS asio)
    roscraft_dep_log(SUCCESS "asio configured (roscraft::asio)")
elseif(TARGET roscraft::asio)
    roscraft_dep_log(SUCCESS "asio configured (roscraft::asio)")
else()
    roscraft_dep_log(NOT_FOUND "asio")
endif()

# Create roscraft::asio convenience target
if(NOT TARGET _roscraft_asio_all)
    add_library(_roscraft_asio_all INTERFACE)
    if(TARGET roscraft::asio)
        target_link_libraries(_roscraft_asio_all INTERFACE roscraft::asio)
    endif()
endif()

if(NOT TARGET roscraft::asio_all)
    add_library(roscraft::asio_all ALIAS _roscraft_asio_all)
endif()
