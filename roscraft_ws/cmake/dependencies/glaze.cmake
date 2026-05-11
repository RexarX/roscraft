# glaze dependency configuration
#
# This module handles finding glaze from multiple sources:
# 1. System packages (if available)
# 2. CPM download (fallback)
#
# Usage:
#   roscraft_require_dependency(glaze)

include_guard(GLOBAL)

# Check if already processed
roscraft_dep_is_processed(NAME "glaze" OUTPUT_VAR _glaze_processed)
if(_glaze_processed)
  return()
endif()

roscraft_dep_header(NAME "glaze")

roscraft_dep_begin(
    NAME glaze
    VERSION ^7.0
    DEBIAN_NAMES glaze-dev
    PACMAN_NAMES glaze
    BREW_NAMES glaze
    PKG_CONFIG_NAMES glaze
    CPM_NAME glaze
    CPM_VERSION 7.6.0
    CPM_GITHUB_REPOSITORY stephenberry/glaze
    CPM_GIT_TAG v7.6.0
)
roscraft_dep_end()

# Create roscraft::glaze::glaze alias
if(NOT TARGET roscraft::glaze::glaze)
  if(TARGET glaze::glaze)
    get_target_property(_glaze_aliased_target glaze::glaze ALIASED_TARGET)
    if(_glaze_aliased_target)
      add_library(roscraft::glaze::glaze ALIAS ${_glaze_aliased_target})
    else()
      add_library(roscraft::glaze::glaze ALIAS glaze::glaze)
    endif()
    roscraft_dep_log(SUCCESS "glaze configured (glaze::glaze)")
  elseif(TARGET glaze)
    add_library(roscraft::glaze::glaze ALIAS glaze)
    roscraft_dep_log(SUCCESS "glaze configured (glaze)")
  elseif(DEFINED glaze_SOURCE_DIR AND EXISTS "${glaze_SOURCE_DIR}/include/glaze/glaze.hpp")
    add_library(_roscraft_glaze_header_only INTERFACE)
    target_include_directories(_roscraft_glaze_header_only SYSTEM INTERFACE
            "${glaze_SOURCE_DIR}/include"
        )
    add_library(roscraft::glaze::glaze ALIAS _roscraft_glaze_header_only)
    roscraft_dep_log(SUCCESS "glaze configured (header-only)")
  else()
    roscraft_dep_log(NOT_FOUND "glaze")
  endif()
else()
  roscraft_dep_log(SUCCESS "glaze configured (roscraft::glaze::glaze)")
endif()

# Create roscraft::glaze convenience target
if(NOT TARGET _roscraft_glaze_all)
  add_library(_roscraft_glaze_all INTERFACE)
  if(TARGET roscraft::glaze::glaze)
    target_link_libraries(_roscraft_glaze_all INTERFACE roscraft::glaze::glaze)
  endif()
endif()

if(NOT TARGET roscraft::glaze)
  add_library(roscraft::glaze ALIAS _roscraft_glaze_all)
endif()
