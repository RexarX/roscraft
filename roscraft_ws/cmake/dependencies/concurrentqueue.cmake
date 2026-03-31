# concurrentqueue dependency configuration
#
# This module handles finding concurrentqueue from multiple sources:
# 1. System packages (pacman, apt, etc.)
# 2. CPM download (fallback)
#
# Usage:
#   roscraft_require_dependency(concurrentqueue)

include_guard(GLOBAL)

# Check if already processed
roscraft_dep_is_processed(NAME "concurrentqueue" OUTPUT_VAR _concurrentqueue_processed)
if(_concurrentqueue_processed)
    return()
endif()

roscraft_dep_header(NAME "concurrentqueue")

# Use roscraft_module system for standard package finding
roscraft_dep_begin(
    NAME concurrentqueue
    VERSION 1.0.4
    DEBIAN_NAMES concurrentqueue-dev
    BREW_NAMES concurrentqueue
    PKG_CONFIG_NAMES concurrentqueue
    CPM_NAME concurrentqueue
    CPM_VERSION 1.0.4
    CPM_GITHUB_REPOSITORY cameron314/concurrentqueue
)
roscraft_dep_end()

# Create roscraft::concurrentqueue::concurrentqueue alias if concurrentqueue was found
if(NOT TARGET roscraft::concurrentqueue::concurrentqueue)
    if(TARGET concurrentqueue::concurrentqueue)
        add_library(roscraft::concurrentqueue::concurrentqueue ALIAS concurrentqueue::concurrentqueue)
        roscraft_dep_log(SUCCESS "concurrentqueue configured (concurrentqueue::concurrentqueue)")
    elseif(TARGET concurrentqueue)
        add_library(roscraft::concurrentqueue::concurrentqueue ALIAS concurrentqueue)
        roscraft_dep_log(SUCCESS "concurrentqueue configured (concurrentqueue)")
    else()
        roscraft_dep_log(NOT_FOUND "concurrentqueue")
    endif()
else()
    roscraft_dep_log(SUCCESS "concurrentqueue configured (roscraft::concurrentqueue::concurrentqueue)")
endif()

# Create roscraft::concurrentqueue convenience target that brings in all concurrentqueue targets
if(NOT TARGET _roscraft_concurrentqueue_all)
    add_library(_roscraft_concurrentqueue_all INTERFACE)
    if(TARGET roscraft::concurrentqueue::concurrentqueue)
        target_link_libraries(_roscraft_concurrentqueue_all INTERFACE roscraft::concurrentqueue::concurrentqueue)
    endif()
endif()

if(NOT TARGET roscraft::concurrentqueue)
    add_library(roscraft::concurrentqueue ALIAS _roscraft_concurrentqueue_all)
endif()
