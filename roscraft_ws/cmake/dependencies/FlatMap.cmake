# FlatMap dependency configuration
#
# This module handles finding flat_map from multiple sources:
# 1. C++23 std::flat_map (if available)
# 2. Boost.Container flat_map (fallback)
#
# Usage:
#   roscraft_require_dependency(FlatMap)

include_guard(GLOBAL)

# Check if already processed
roscraft_dep_is_processed(NAME "FlatMap" OUTPUT_VAR _flatmap_processed)
if(_flatmap_processed)
  return()
endif()

roscraft_dep_header(NAME "FlatMap")

# Check if C++23 std::flat_map is available
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS}")

check_cxx_source_compiles("
#include <flat_map>
#include <string>

int main() {
    std::flat_map<int, std::string> map;
    map.insert({1, \"test\"});
    auto it = map.find(1);
    return it != map.end() ? 0 : 1;
}
" roscraft_HAS_STL_FLAT_MAP)

cmake_pop_check_state()

if(roscraft_HAS_STL_FLAT_MAP)
  roscraft_dep_log(SUCCESS "C++23 std::flat_map available, using STL flat_map")
  set(roscraft_USE_STL_FLAT_MAP ON CACHE INTERNAL "Use C++23 STL flat_map instead of Boost")

  # Create a target for STL flat_map
  if(NOT TARGET roscraft::stl_flat_map)
    add_library(roscraft::stl_flat_map INTERFACE IMPORTED GLOBAL)
    target_compile_definitions(roscraft::stl_flat_map INTERFACE roscraft_USE_STL_FLAT_MAP)
  endif()

  # Create roscraft::flat_map as alias to STL flat_map
  if(NOT TARGET roscraft::flat_map)
    add_library(roscraft::flat_map INTERFACE IMPORTED GLOBAL)
    target_link_libraries(roscraft::flat_map INTERFACE roscraft::stl_flat_map)
  endif()

  roscraft_dep_mark_found(NAME "FlatMap" VIA "STL (C++23)")
  roscraft_dep_mark_processed(NAME "FlatMap")
else()
  roscraft_dep_log(STATUS "C++23 std::flat_map not available, using Boost.Container flat_map")
  set(roscraft_USE_STL_FLAT_MAP OFF CACHE INTERNAL "Use C++23 STL flat_map instead of Boost")

  # Require Boost dependency (this will handle finding/downloading Boost)
  roscraft_require_dependency(Boost)

  # Check if Boost was found
  if(TARGET Boost::boost OR TARGET roscraft::boost::boost)
    # Create roscraft::boost::container target for flat_map
    if(NOT TARGET roscraft::boost::container)
      add_library(roscraft::boost::container INTERFACE IMPORTED GLOBAL)

      if(TARGET Boost::container)
        target_link_libraries(roscraft::boost::container INTERFACE Boost::container)
      elseif(TARGET roscraft::boost::boost)
        # Boost.Container is header-only for flat_map
        target_link_libraries(roscraft::boost::container INTERFACE roscraft::boost::boost)

        # Add container include path if using CPM-downloaded Boost
        if(Boost_SOURCE_DIR AND EXISTS "${Boost_SOURCE_DIR}/libs/container/include")
          target_include_directories(roscraft::boost::container SYSTEM INTERFACE
                        "${Boost_SOURCE_DIR}/libs/container/include"
                    )
        endif()
      endif()
    endif()

    # Create roscraft::flat_map as alias to Boost container
    if(NOT TARGET roscraft::flat_map)
      add_library(roscraft::flat_map INTERFACE IMPORTED GLOBAL)
      target_link_libraries(roscraft::flat_map INTERFACE roscraft::boost::container)
      if(TARGET roscraft::boost::boost)
        target_link_libraries(roscraft::flat_map INTERFACE roscraft::boost::boost)
      endif()
    endif()

    roscraft_dep_mark_found(NAME "FlatMap" VIA "Boost.Container")
    roscraft_dep_mark_processed(NAME "FlatMap")
  else()
    roscraft_dep_log(WARNING "FlatMap: Neither std::flat_map nor Boost.Container available")
    roscraft_dep_mark_processed(NAME "FlatMap")
  endif()
endif()
