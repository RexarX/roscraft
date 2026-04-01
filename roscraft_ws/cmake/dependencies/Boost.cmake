# Boost dependency configuration
#
# This module handles finding Boost from multiple sources:
# 1. System packages (pacman, apt, etc.)
# 2. CPM download (fallback)
#
# Additionally, it checks for C++23 <stacktrace> header availability
# and uses STL stacktrace instead of Boost if available.
#
# Usage:
#   roscraft_require_dependency(Boost)

include_guard(GLOBAL)

# Check if already processed
roscraft_dep_is_processed(NAME "Boost" OUTPUT_VAR _boost_processed)
if(_boost_processed)
    return()
endif()

roscraft_dep_header(NAME "Boost")

# Check if C++23 <stacktrace> header is available
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_REQUIRED_LINK_OPTIONS "")

# Try to detect the compiler and set appropriate flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC requires libstdc++_libbacktrace for std::stacktrace
    list(APPEND CMAKE_REQUIRED_LINK_OPTIONS "-lstdc++_libbacktrace")
    set(_stl_stacktrace_link_libs "stdc++_libbacktrace")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Clang with libc++ may need different handling
    if(CMAKE_CXX_FLAGS MATCHES "-stdlib=libc\\+\\+")
        # libc++ doesn't have full stacktrace support yet in most versions
        set(_stl_stacktrace_link_libs "")
    else()
        # Clang with libstdc++
        list(APPEND CMAKE_REQUIRED_LINK_OPTIONS "-lstdc++_libbacktrace")
        set(_stl_stacktrace_link_libs "stdc++_libbacktrace")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # MSVC has built-in support, no extra libs needed
    set(_stl_stacktrace_link_libs "")
endif()

check_cxx_source_compiles("
#include <stacktrace>
#include <string>

int main() {
    auto st = std::stacktrace::current();
    if (st.size() > 0) {
        std::string desc = std::to_string(st[0]);
        (void)desc;
    }
    return 0;
}
" ROSCRAFT_HAS_STL_STACKTRACE)

cmake_pop_check_state()

if(ROSCRAFT_HAS_STL_STACKTRACE)
    roscraft_dep_log(SUCCESS "C++23 <stacktrace> header available, using STL stacktrace")
    set(ROSCRAFT_USE_STL_STACKTRACE ON CACHE INTERNAL "Use C++23 STL stacktrace instead of Boost")

    # Create a target for STL stacktrace
    if(NOT TARGET roscraft::stl_stacktrace)
        add_library(roscraft::stl_stacktrace INTERFACE IMPORTED GLOBAL)
        target_compile_definitions(roscraft::stl_stacktrace INTERFACE ROSCRAFT_USE_STL_STACKTRACE)

        # Link required libraries for STL stacktrace
        if(_stl_stacktrace_link_libs)
            target_link_libraries(roscraft::stl_stacktrace INTERFACE ${_stl_stacktrace_link_libs})
        endif()
    endif()

    # Create roscraft::boost::stacktrace as alias to STL stacktrace
    # This allows existing code to link against roscraft::boost::stacktrace without changes
    if(NOT TARGET roscraft::boost::stacktrace)
        add_library(roscraft::boost::stacktrace INTERFACE IMPORTED GLOBAL)
        target_link_libraries(roscraft::boost::stacktrace INTERFACE roscraft::stl_stacktrace)
    endif()
else()
    roscraft_dep_log(STATUS "C++23 <stacktrace> not available, using Boost stacktrace")
    set(ROSCRAFT_USE_STL_STACKTRACE OFF CACHE INTERNAL "Use C++23 STL stacktrace instead of Boost")
endif()

# Boost components required by Roscraft
# Note: unordered is header-only and doesn't need to be in this list
# stacktrace is only required if STL stacktrace is not available
if(NOT ROSCRAFT_USE_STL_STACKTRACE)
    set(ROSCRAFT_BOOST_REQUIRED_COMPONENTS
        stacktrace
    )
else()
    set(ROSCRAFT_BOOST_REQUIRED_COMPONENTS "")
endif()

# Try to find Boost in order of preference
set(_boost_found_via "")
set(_boost_prefer_cpm FALSE)

# If Boost was resolved via CPM in this build tree before, skip repeated
# expensive system probing on subsequent configure runs.
if(DEFINED ROSCRAFT_DEP_BOOST_FOUND_VIA AND ROSCRAFT_DEP_BOOST_FOUND_VIA STREQUAL "CPM")
    set(_boost_prefer_cpm TRUE)
endif()

# 1. Try system package manager first
# Try CONFIG mode first (modern CMake packages like Arch Linux, Ubuntu 22.04+)
if(NOT _boost_prefer_cpm)
    find_package(Boost 1.83 QUIET CONFIG)
    if(Boost_FOUND)
        set(_boost_found_via "system (CONFIG)")
    else()
        # Fall back to MODULE mode with specific components
        if(ROSCRAFT_BOOST_REQUIRED_COMPONENTS)
            find_package(Boost 1.83 QUIET COMPONENTS ${ROSCRAFT_BOOST_REQUIRED_COMPONENTS})
        else()
            find_package(Boost 1.83 QUIET)
        endif()
        if(Boost_FOUND)
            set(_boost_found_via "system (MODULE)")
        endif()
    endif()
endif()

# 3. Create Boost targets if found
if(Boost_FOUND)
    if(_boost_found_via)
        roscraft_dep_log(SUCCESS "Boost found via ${_boost_found_via}")
        roscraft_dep_mark_found(NAME "Boost" VIA "${_boost_found_via}")
    else()
        roscraft_dep_log(SUCCESS "Boost found at ${Boost_DIR}")
        roscraft_dep_mark_found(NAME "Boost" VIA "system")
    endif()

    # Create convenience target: roscraft::boost::boost (main headers)
    if(NOT TARGET roscraft::boost::boost)
        add_library(roscraft::boost::boost INTERFACE IMPORTED)
        target_link_libraries(roscraft::boost::boost INTERFACE Boost::boost)
    endif()

    # Create stacktrace-specific alias: roscraft::boost::stacktrace (only if not using STL stacktrace)
    if(NOT ROSCRAFT_USE_STL_STACKTRACE)
        if(NOT TARGET roscraft::boost::stacktrace)
            add_library(roscraft::boost::stacktrace INTERFACE IMPORTED GLOBAL)
            if(TARGET Boost::stacktrace)
                target_link_libraries(roscraft::boost::stacktrace INTERFACE Boost::stacktrace)
            endif()
        endif()

        if(TARGET Boost::stacktrace_basic AND NOT TARGET roscraft::boost::stacktrace_basic)
            add_library(roscraft::boost::stacktrace_basic INTERFACE IMPORTED GLOBAL)
            target_link_libraries(roscraft::boost::stacktrace_basic INTERFACE Boost::stacktrace_basic)
        endif()

        if(TARGET Boost::stacktrace_backtrace AND NOT TARGET roscraft::boost::stacktrace_backtrace)
            add_library(roscraft::boost::stacktrace_backtrace INTERFACE IMPORTED GLOBAL)
            target_link_libraries(roscraft::boost::stacktrace_backtrace INTERFACE Boost::stacktrace_backtrace)
        endif()

        if(TARGET Boost::stacktrace_addr2line AND NOT TARGET roscraft::boost::stacktrace_addr2line)
            add_library(roscraft::boost::stacktrace_addr2line INTERFACE IMPORTED GLOBAL)
            target_link_libraries(roscraft::boost::stacktrace_addr2line INTERFACE Boost::stacktrace_addr2line)
        endif()

        if(TARGET Boost::stacktrace_windbg AND NOT TARGET roscraft::boost::stacktrace_windbg)
            add_library(roscraft::boost::stacktrace_windbg INTERFACE IMPORTED GLOBAL)
            target_link_libraries(roscraft::boost::stacktrace_windbg INTERFACE Boost::stacktrace_windbg)
        endif()
    endif()

    # Create alias for header-only unordered (not a separate component)
    if(NOT TARGET roscraft::boost::unordered)
        add_library(roscraft::boost::unordered INTERFACE IMPORTED GLOBAL)
        target_link_libraries(roscraft::boost::unordered INTERFACE Boost::boost)

        # Manually add unordered include path if found via system
        if(Boost_INCLUDE_DIRS)
            # System Boost should already have all headers accessible
            # No need to add extra paths
        endif()
    endif()

    # Mark as processed
    roscraft_dep_mark_processed(NAME "Boost")
else()
    # 4. Try CPM fallback if system packages not found
    if(ROSCRAFT_${MODULE_NAME}_DOWNLOAD_PACKAGES)
        roscraft_dep_log(DOWNLOAD "Boost not found in system, downloading via CPM...")

        # Only include stacktrace in download if STL stacktrace is not available
        # Always include container for flat_map support
        if(ROSCRAFT_USE_STL_STACKTRACE)
            set(_boost_include_libs "container;unordered")
        else()
            set(_boost_include_libs "container;stacktrace;unordered")
        endif()

        include(DownloadUsingCPM)
        roscraft_cpm_add_package(
            NAME Boost
            VERSION 1.90.0
            URL https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0-cmake.tar.xz
            OPTIONS
                "BOOST_ENABLE_CMAKE ON"
                "BOOST_INCLUDE_LIBRARIES ${_boost_include_libs}"
                "BUILD_SHARED_LIBS OFF"
                "CMAKE_POSITION_INDEPENDENT_CODE ON"
            SYSTEM
        )

        # Create aliases if Boost was just added or already cached
        if(Boost_ADDED OR TARGET Boost::boost)
            if(NOT TARGET roscraft::boost::boost)
                add_library(roscraft::boost::boost INTERFACE IMPORTED GLOBAL)
                if(TARGET Boost::boost)
                    target_link_libraries(roscraft::boost::boost INTERFACE Boost::boost)
                endif()
            endif()

            # Only create Boost stacktrace target if not using STL stacktrace
            if(NOT ROSCRAFT_USE_STL_STACKTRACE)
                if(NOT TARGET roscraft::boost::stacktrace)
                    add_library(roscraft::boost::stacktrace INTERFACE IMPORTED GLOBAL)
                endif()

                if(TARGET Boost::stacktrace)
                    target_link_libraries(roscraft::boost::stacktrace INTERFACE Boost::stacktrace)
                else()
                    target_link_libraries(roscraft::boost::stacktrace INTERFACE roscraft::boost::boost)
                endif()

                if(TARGET Boost::stacktrace_backtrace AND NOT TARGET roscraft::boost::stacktrace_backtrace)
                    add_library(roscraft::boost::stacktrace_backtrace INTERFACE IMPORTED GLOBAL)
                    target_link_libraries(roscraft::boost::stacktrace_backtrace INTERFACE Boost::stacktrace_backtrace)
                endif()

                if(TARGET Boost::stacktrace_addr2line AND NOT TARGET roscraft::boost::stacktrace_addr2line)
                    add_library(roscraft::boost::stacktrace_addr2line INTERFACE IMPORTED GLOBAL)
                    target_link_libraries(roscraft::boost::stacktrace_addr2line INTERFACE Boost::stacktrace_addr2line)
                endif()
            endif()

            # Create alias for header-only unordered (not a separate component)
            if(NOT TARGET roscraft::boost::unordered)
                add_library(roscraft::boost::unordered INTERFACE IMPORTED GLOBAL)
                target_link_libraries(roscraft::boost::unordered INTERFACE roscraft::boost::boost)

                # Manually add unordered include path since BOOST_INCLUDE_LIBRARIES doesn't always set it up
                if(Boost_SOURCE_DIR AND EXISTS "${Boost_SOURCE_DIR}/libs/unordered/include")
                    target_include_directories(roscraft::boost::unordered SYSTEM INTERFACE
                        "${Boost_SOURCE_DIR}/libs/unordered/include"
                    )
                endif()
            endif()

            # Create alias for container (needed for flat_map)
            if(NOT TARGET roscraft::boost::container)
                add_library(roscraft::boost::container INTERFACE IMPORTED GLOBAL)
                if(TARGET Boost::container)
                    target_link_libraries(roscraft::boost::container INTERFACE Boost::container)
                else()
                    target_link_libraries(roscraft::boost::container INTERFACE roscraft::boost::boost)
                endif()

                # Manually add container include path since BOOST_INCLUDE_LIBRARIES doesn't always set it up
                if(Boost_SOURCE_DIR AND EXISTS "${Boost_SOURCE_DIR}/libs/container/include")
                    target_include_directories(roscraft::boost::container SYSTEM INTERFACE
                        "${Boost_SOURCE_DIR}/libs/container/include"
                    )
                endif()
            endif()

            # Mark Boost targets as SYSTEM to suppress warnings
            if(TARGET Boost::boost)
                roscraft_cpm_mark_as_system(Boost::boost)
            endif()
            if(TARGET Boost::stacktrace)
                roscraft_cpm_mark_as_system(Boost::stacktrace)
            endif()
            if(TARGET Boost::stacktrace_backtrace)
                roscraft_cpm_mark_as_system(Boost::stacktrace_backtrace)
            endif()
            if(TARGET Boost::stacktrace_addr2line)
                roscraft_cpm_mark_as_system(Boost::stacktrace_addr2line)
            endif()
            if(TARGET Boost::stacktrace_basic)
                roscraft_cpm_mark_as_system(Boost::stacktrace_basic)
            endif()
            if(TARGET Boost::container)
                roscraft_cpm_mark_as_system(Boost::container)
            endif()
        endif()

        # Mark as found and processed
        roscraft_dep_mark_processed(NAME "Boost")
        roscraft_dep_mark_found(NAME "Boost" VIA "CPM")
        list(APPEND CPM_PACKAGES "Boost")
    else()
        roscraft_dep_log(WARNING "Boost not found and ROSCRAFT_${MODULE_NAME}_DOWNLOAD_PACKAGES is OFF")
        roscraft_dep_mark_processed(NAME "Boost")
    endif()
endif()

# Dynamically create aliases for each Boost component (only if not using STL stacktrace for stacktrace component)
foreach(component IN LISTS ROSCRAFT_BOOST_REQUIRED_COMPONENTS)
    set(target_name "roscraft::boost::${component}")

    if(TARGET "Boost::${component}")
        if(NOT TARGET ${target_name})
            add_library(${target_name} INTERFACE IMPORTED GLOBAL)
            target_link_libraries(${target_name} INTERFACE "Boost::${component}")
            roscraft_dep_log(STATUS "Created alias: ${target_name}")
        endif()
        # Link the component to the main roscraft::boost::boost target (excluding pool and container)
        if(NOT component STREQUAL "pool" AND NOT component STREQUAL "container")
            target_link_libraries(roscraft::boost::boost INTERFACE "Boost::${component}")
        endif()
    else()
        if(NOT ROSCRAFT_USE_STL_STACKTRACE OR NOT component STREQUAL "stacktrace")
            roscraft_dep_log(WARNING "Boost component '${component}' not found")
        endif()
    endif()
endforeach()

# Create roscraft::boost convenience target that brings in all Boost targets
if(NOT TARGET _ROSCRAFT_boost_all)
    add_library(_ROSCRAFT_boost_all INTERFACE)
    if(TARGET roscraft::boost::boost)
        target_link_libraries(_ROSCRAFT_boost_all INTERFACE roscraft::boost::boost)
    endif()
    if(TARGET roscraft::boost::unordered)
        target_link_libraries(_ROSCRAFT_boost_all INTERFACE roscraft::boost::unordered)
    endif()
    if(TARGET roscraft::boost::stacktrace)
        target_link_libraries(_ROSCRAFT_boost_all INTERFACE roscraft::boost::stacktrace)
    endif()
endif()

if(NOT TARGET roscraft::boost)
    add_library(roscraft::boost ALIAS _ROSCRAFT_boost_all)
endif()
