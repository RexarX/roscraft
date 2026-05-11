# FlatBuffers dependency configuration
#
# This module handles finding FlatBuffers from multiple sources:
# 1. System packages (pacman, apt, etc.)
# 2. CPM download (fallback)
#
# Usage:
#   roscraft_require_dependency(FlatBuffers)

include_guard(GLOBAL)

# Check if already processed
roscraft_dep_is_processed(NAME "flatbuffers" OUTPUT_VAR _flatbuffers_processed)
if(_flatbuffers_processed)
  return()
endif()

roscraft_dep_header(NAME "flatbuffers")

# Pre-create roscraft::flatbuffers as an IMPORTED GLOBAL target so that
# roscraft_dep_end does not create an internal wrapper.  IMPORTED GLOBAL
# targets (unlike aliases) store their own name in export sets, and their
# ::-qualified name is permitted for IMPORTED targets.
if(NOT TARGET roscraft::flatbuffers)
  add_library(roscraft::flatbuffers INTERFACE IMPORTED GLOBAL)
endif()

roscraft_dep_begin(
    NAME flatbuffers
    VERSION ^25.12
    DEBIAN_NAMES flatbuffers-dev
    PACMAN_NAMES flatbuffers
    BREW_NAMES flatbuffers
    PKG_CONFIG_NAMES flatbuffers
    CPM_NAME flatbuffers
    CPM_VERSION 25.12.19
    CPM_GITHUB_REPOSITORY google/flatbuffers
    CPM_GIT_TAG v25.12.19-2026-02-06-03fffb2
    CPM_OPTIONS
        "FLATBUFFERS_BUILD_TESTS OFF"
)
roscraft_dep_end()

# Create roscraft::flatbuffers::flatbuffers alias if flatbuffers was found
if(NOT TARGET roscraft::flatbuffers::flatbuffers)
  if(TARGET flatbuffers::flatbuffers)
    add_library(roscraft::flatbuffers::flatbuffers ALIAS flatbuffers::flatbuffers)
    roscraft_dep_log(SUCCESS "FlatBuffers configured (flatbuffers::flatbuffers)")
  elseif(TARGET flatbuffers::flatbuffers_shared)
    add_library(roscraft::flatbuffers::flatbuffers ALIAS flatbuffers::flatbuffers_shared)
    roscraft_dep_log(SUCCESS "FlatBuffers configured (flatbuffers::flatbuffers_shared)")
  elseif(TARGET flatbuffers::libflatbuffers)
    add_library(roscraft::flatbuffers::flatbuffers ALIAS flatbuffers::libflatbuffers)
    roscraft_dep_log(SUCCESS "FlatBuffers configured (flatbuffers::libflatbuffers)")
  elseif(TARGET FlatBuffers::FlatBuffers)
    get_target_property(_flatbuffers_aliased_target FlatBuffers::FlatBuffers ALIASED_TARGET)
    if(_flatbuffers_aliased_target)
      add_library(roscraft::flatbuffers::flatbuffers ALIAS ${_flatbuffers_aliased_target})
    else()
      add_library(roscraft::flatbuffers::flatbuffers ALIAS FlatBuffers::FlatBuffers)
    endif()
    roscraft_dep_log(SUCCESS "FlatBuffers configured (FlatBuffers::FlatBuffers)")
  elseif(TARGET flatbuffers)
    add_library(roscraft::flatbuffers::flatbuffers ALIAS flatbuffers)
    roscraft_dep_log(SUCCESS "FlatBuffers configured (flatbuffers)")
  elseif(DEFINED flatbuffers_SOURCE_DIR AND EXISTS "${flatbuffers_SOURCE_DIR}/include/flatbuffers/flatbuffers.h")
    add_library(_roscraft_flatbuffers_header_only INTERFACE)
    target_include_directories(_roscraft_flatbuffers_header_only SYSTEM INTERFACE
            "${flatbuffers_SOURCE_DIR}/include"
        )
    add_library(roscraft::flatbuffers::flatbuffers ALIAS _roscraft_flatbuffers_header_only)
    roscraft_dep_log(SUCCESS "FlatBuffers configured (header-only)")
  else()
    roscraft_dep_log(NOT_FOUND "FlatBuffers")
  endif()
else()
  roscraft_dep_log(SUCCESS "FlatBuffers configured (roscraft::flatbuffers::flatbuffers)")
endif()

# Wire up the pre-created roscraft::flatbuffers target with the real flatbuffers library.
if(TARGET roscraft::flatbuffers::flatbuffers)
  get_target_property(_roscraft_fb_aliased roscraft::flatbuffers::flatbuffers ALIASED_TARGET)
  if(_roscraft_fb_aliased)
    set(_roscraft_fb_lib_target "${_roscraft_fb_aliased}")
  else()
    set(_roscraft_fb_lib_target "roscraft::flatbuffers::flatbuffers")
  endif()

  target_link_libraries(roscraft::flatbuffers INTERFACE ${_roscraft_fb_lib_target})

  get_target_property(_target_includes ${_roscraft_fb_lib_target} INTERFACE_INCLUDE_DIRECTORIES)
  if(_target_includes)
    set_target_properties(roscraft::flatbuffers PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_target_includes}"
    )
  endif()

  unset(_roscraft_fb_aliased)
  unset(_roscraft_fb_lib_target)
  unset(_target_includes)
endif()
