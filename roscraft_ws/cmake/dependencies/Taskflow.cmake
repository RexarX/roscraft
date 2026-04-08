# Taskflow dependency configuration
#
# This module handles finding Taskflow from multiple sources:
# 1. System packages (pacman, apt, etc.)
# 2. CPM download (fallback)
#
# Usage in plugin CMakeLists.txt:
#   roscraft_require_dependency(Taskflow)

include_guard(GLOBAL)

# Check if already processed
roscraft_dep_is_processed(NAME "Taskflow" OUTPUT_VAR _taskflow_processed)
if(_taskflow_processed)
  return()
endif()

roscraft_dep_header(NAME "Taskflow")

# Use roscraft_module system for standard package finding
roscraft_dep_begin(
    NAME Taskflow
    VERSION 4.0.0
    DEBIAN_NAMES libtaskflow-cpp-dev
    RPM_NAMES taskflow-devel
    PACMAN_NAMES taskflow
    BREW_NAMES taskflow
    PKG_CONFIG_NAMES taskflow
    CPM_NAME taskflow
    CPM_VERSION 4.0.0
    CPM_GITHUB_REPOSITORY taskflow/taskflow
    CPM_GIT_TAG v4.0.0
    CPM_OPTIONS
        "TF_BUILD_TESTS OFF"
        "TF_BUILD_EXAMPLES OFF"
)

roscraft_dep_end()

# Create roscraft::taskflow::taskflow alias if Taskflow was found
if(NOT TARGET roscraft::taskflow::taskflow)
  if(TARGET Taskflow::Taskflow)
    # Taskflow::Taskflow might be an alias itself, get the real target
    get_target_property(_taskflow_aliased Taskflow::Taskflow ALIASED_TARGET)
    if(_taskflow_aliased)
      add_library(roscraft::taskflow::taskflow ALIAS ${_taskflow_aliased})
    else()
      add_library(roscraft::taskflow::taskflow ALIAS Taskflow::Taskflow)
    endif()
    roscraft_dep_log(SUCCESS "Taskflow configured (Taskflow::Taskflow)")

    # Mark Taskflow targets as SYSTEM to suppress warnings if downloaded via CPM
    if(taskflow_ADDED)
      if(TARGET Taskflow::Taskflow)
        roscraft_cpm_mark_as_system(Taskflow::Taskflow)
      endif()
      if(TARGET Taskflow)
        roscraft_cpm_mark_as_system(Taskflow)
      endif()
    endif()
  elseif(TARGET Taskflow)
    add_library(roscraft::taskflow::taskflow ALIAS Taskflow)
    roscraft_dep_log(SUCCESS "Taskflow configured (Taskflow)")

    # Mark Taskflow targets as SYSTEM to suppress warnings if downloaded via CPM
    if(taskflow_ADDED)
      roscraft_cpm_mark_as_system(Taskflow)
    endif()
  else()
    roscraft_dep_log(NOT_FOUND "Taskflow")
  endif()
else()
  roscraft_dep_log(SUCCESS "Taskflow configured (roscraft::taskflow::taskflow)")
endif()

# Create roscraft::taskflow convenience target that brings in all taskflow targets
if(NOT TARGET _roscraft_taskflow_all)
  add_library(_roscraft_taskflow_all INTERFACE)
  if(TARGET roscraft::taskflow::taskflow)
    target_link_libraries(_roscraft_taskflow_all INTERFACE roscraft::taskflow::taskflow)
  endif()
endif()

if(NOT TARGET roscraft::taskflow)
  add_library(roscraft::taskflow ALIAS _roscraft_taskflow_all)
endif()
