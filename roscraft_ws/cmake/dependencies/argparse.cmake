# argparse dependency configuration
#
# This module handles finding argparse from multiple sources:
# 1. System packages (if available)
# 2. CPM download (fallback)
#
# Usage:
#   roscraft_require_dependency(argparse)

include_guard(GLOBAL)

# Check if already processed
roscraft_dep_is_processed(NAME "argparse" OUTPUT_VAR _argparse_processed)
if(_argparse_processed)
  return()
endif()

roscraft_dep_header(NAME "argparse")

roscraft_dep_begin(
    NAME argparse
    VERSION ^3.0
    DEBIAN_NAMES argparse-dev
    RPM_NAMES argparse-devel
    PACMAN_NAMES argparse
    BREW_NAMES argparse
    PKG_CONFIG_NAMES argparse
    CPM_NAME argparse
    CPM_VERSION 3.2
    CPM_GITHUB_REPOSITORY p-ranav/argparse
    CPM_GIT_TAG v3.2
)
roscraft_dep_end()

# Create roscraft::argparse::argparse alias
if(NOT TARGET roscraft::argparse::argparse)
  if(TARGET argparse::argparse)
    get_target_property(_argparse_aliased_target argparse::argparse ALIASED_TARGET)
    if(_argparse_aliased_target)
      add_library(roscraft::argparse::argparse ALIAS ${_argparse_aliased_target})
    else()
      add_library(roscraft::argparse::argparse ALIAS argparse::argparse)
    endif()
    roscraft_dep_log(SUCCESS "argparse configured (argparse::argparse)")
  elseif(TARGET argparse)
    add_library(roscraft::argparse::argparse ALIAS argparse)
    roscraft_dep_log(SUCCESS "argparse configured (argparse)")
  elseif(DEFINED argparse_SOURCE_DIR AND EXISTS "${argparse_SOURCE_DIR}/include/argparse/argparse.hpp")
    add_library(_roscraft_argparse_header_only INTERFACE)
    target_include_directories(_roscraft_argparse_header_only SYSTEM INTERFACE
            "${argparse_SOURCE_DIR}/include"
        )
    add_library(roscraft::argparse::argparse ALIAS _roscraft_argparse_header_only)
    roscraft_dep_log(SUCCESS "argparse configured (header-only)")
  else()
    roscraft_dep_log(NOT_FOUND "argparse")
  endif()
else()
  roscraft_dep_log(SUCCESS "argparse configured (roscraft::argparse::argparse)")
endif()

# Create roscraft::argparse convenience target
if(NOT TARGET _roscraft_argparse_all)
  add_library(_roscraft_argparse_all INTERFACE)
  if(TARGET roscraft::argparse::argparse)
    target_link_libraries(_roscraft_argparse_all INTERFACE roscraft::argparse::argparse)
  endif()
endif()

if(NOT TARGET roscraft::argparse)
  add_library(roscraft::argparse ALIAS _roscraft_argparse_all)
endif()
