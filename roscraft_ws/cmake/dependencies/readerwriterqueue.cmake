# readerwriterqueue dependency configuration
#
# This module handles finding readerwriterqueue from multiple sources:
# 1. System packages (if available)
# 2. CPM download (fallback)
#
# Usage:
#   roscraft_require_dependency(readerwriterqueue)

include_guard(GLOBAL)

set(ROSCRAFT_READERWRITERQUEUE_REQUIRED_VERSION_PATTERN "^1\\.0\\..*")

# Check if already processed
roscraft_dep_is_processed(NAME "readerwriterqueue" OUTPUT_VAR _readerwriterqueue_processed)
if(_readerwriterqueue_processed)
    return()
endif()

roscraft_dep_header(NAME "readerwriterqueue")

roscraft_dep_begin(
    NAME readerwriterqueue
    VERSION 1.0.0
    DEBIAN_NAMES readerwriterqueue-dev
    BREW_NAMES readerwriterqueue
    PKG_CONFIG_NAMES readerwriterqueue
    CPM_NAME readerwriterqueue
    CPM_VERSION 1.0.7
    CPM_GITHUB_REPOSITORY cameron314/readerwriterqueue
    CPM_GIT_TAG v1.0.7
)
roscraft_dep_end()

set(_readerwriterqueue_detected_version "")
if(DEFINED readerwriterqueue_VERSION)
    set(_readerwriterqueue_detected_version "${readerwriterqueue_VERSION}")
elseif(DEFINED readerwriterqueue_PC_VERSION)
    set(_readerwriterqueue_detected_version "${readerwriterqueue_PC_VERSION}")
endif()

if(_readerwriterqueue_detected_version)
    if(NOT _readerwriterqueue_detected_version MATCHES "${ROSCRAFT_READERWRITERQUEUE_REQUIRED_VERSION_PATTERN}")
        message(FATAL_ERROR
            "readerwriterqueue version ${_readerwriterqueue_detected_version} is unsupported. "
            "Required version series: 1.0.*"
        )
    endif()
endif()

# Create roscraft::readerwriterqueue::readerwriterqueue alias
if(NOT TARGET roscraft::readerwriterqueue::readerwriterqueue)
    if(TARGET readerwriterqueue::readerwriterqueue)
        add_library(roscraft::readerwriterqueue::readerwriterqueue ALIAS readerwriterqueue::readerwriterqueue)
        roscraft_dep_log(SUCCESS "readerwriterqueue configured (readerwriterqueue::readerwriterqueue)")
    elseif(TARGET readerwriterqueue)
        add_library(roscraft::readerwriterqueue::readerwriterqueue ALIAS readerwriterqueue)
        roscraft_dep_log(SUCCESS "readerwriterqueue configured (readerwriterqueue)")
    elseif(DEFINED readerwriterqueue_SOURCE_DIR AND EXISTS "${readerwriterqueue_SOURCE_DIR}/readerwriterqueue.h")
        add_library(_roscraft_readerwriterqueue_header_only INTERFACE)
        target_include_directories(_roscraft_readerwriterqueue_header_only SYSTEM INTERFACE
            "${readerwriterqueue_SOURCE_DIR}"
        )
        add_library(roscraft::readerwriterqueue::readerwriterqueue ALIAS _roscraft_readerwriterqueue_header_only)
        roscraft_dep_log(SUCCESS "readerwriterqueue configured (header-only)")
    else()
        roscraft_dep_log(NOT_FOUND "readerwriterqueue")
    endif()
else()
    roscraft_dep_log(SUCCESS "readerwriterqueue configured (roscraft::readerwriterqueue::readerwriterqueue)")
endif()

# Create roscraft::readerwriterqueue convenience target
if(NOT TARGET _roscraft_readerwriterqueue_all)
    add_library(_roscraft_readerwriterqueue_all INTERFACE)
    if(TARGET roscraft::readerwriterqueue::readerwriterqueue)
        target_link_libraries(_roscraft_readerwriterqueue_all INTERFACE roscraft::readerwriterqueue::readerwriterqueue)
    endif()
endif()

if(NOT TARGET roscraft::readerwriterqueue)
    add_library(roscraft::readerwriterqueue ALIAS _roscraft_readerwriterqueue_all)
endif()

unset(_readerwriterqueue_detected_version)
