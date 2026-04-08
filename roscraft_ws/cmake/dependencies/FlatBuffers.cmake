# FlatBuffers dependency configuration
#
# This module handles finding FlatBuffers from multiple sources:
# 1. System packages (pacman, apt, etc.)
# 2. CPM download (fallback)
#
# Usage:
#   roscraft_require_dependency(FlatBuffers)

include_guard(GLOBAL)

set(ROSCRAFT_FLATBUFFERS_REQUIRED_VERSION_PATTERN "^25\\.12\\..*")

# Check if already processed
roscraft_dep_is_processed(NAME "flatbuffers" OUTPUT_VAR _flatbuffers_processed)
if(_flatbuffers_processed)
  return()
endif()

roscraft_dep_header(NAME "flatbuffers")

roscraft_dep_begin(
    NAME flatbuffers
    VERSION 25.12.19
    DEBIAN_NAMES flatbuffers-dev
    PACMAN_NAMES flatbuffers
    BREW_NAMES flatbuffers
    PKG_CONFIG_NAMES flatbuffers
    CPM_NAME flatbuffers
    CPM_VERSION 25.12.19
    CPM_GITHUB_REPOSITORY google/flatbuffers
    CPM_GIT_TAG v25.12.19-2026-02-06-03fffb2
)
roscraft_dep_end()

set(_flatbuffers_detected_version "")
if(DEFINED flatbuffers_VERSION)
  set(_flatbuffers_detected_version "${flatbuffers_VERSION}")
elseif(DEFINED flatbuffers_PC_VERSION)
  set(_flatbuffers_detected_version "${flatbuffers_PC_VERSION}")
endif()

if(_flatbuffers_detected_version)
  if(NOT _flatbuffers_detected_version MATCHES "${ROSCRAFT_FLATBUFFERS_REQUIRED_VERSION_PATTERN}")
    message(WARNING
            "FlatBuffers version ${_flatbuffers_detected_version} does not match the preferred "
            "version series 25.12.*. Continuing with detected version."
        )
  endif()
endif()

# Create roscraft::flatbuffers::flatbuffers alias if flatbuffers was found
if(NOT TARGET roscraft::flatbuffers::flatbuffers)
  if(TARGET flatbuffers::flatbuffers)
    add_library(roscraft::flatbuffers::flatbuffers ALIAS flatbuffers::flatbuffers)
    roscraft_dep_log(SUCCESS "FlatBuffers configured (flatbuffers::flatbuffers)")
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

# Create roscraft::flatbuffers convenience target that brings in all FlatBuffers targets
if(NOT TARGET _roscraft_flatbuffers_all)
  add_library(_roscraft_flatbuffers_all INTERFACE)
  if(TARGET roscraft::flatbuffers::flatbuffers)
    target_link_libraries(_roscraft_flatbuffers_all INTERFACE roscraft::flatbuffers::flatbuffers)
  endif()
endif()

if(NOT TARGET roscraft::flatbuffers)
  add_library(roscraft::flatbuffers ALIAS _roscraft_flatbuffers_all)
endif()

unset(_flatbuffers_detected_version)
