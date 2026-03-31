cmake_minimum_required(VERSION 3.25)

project(roscraft_core
    VERSION 0.1.0
    DESCRIPTION "Roscraft core module"
    HOMEPAGE_URL "https://github.com/RexarX/roscraft"
    LANGUAGES CXX
)

# Set archiver tools for Clang to support LTO
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        # On Windows with clang-cl, use llvm-lib (MSVC-compatible) or MSVC's lib.exe
        find_program(LLVM_LIB_EXECUTABLE
            NAMES llvm-lib llvm-lib.exe
            HINTS
                "$ENV{VCINSTALLDIR}/Tools/Llvm/x64/bin"
                "$ENV{VCINSTALLDIR}/Tools/Llvm/bin"
                "C:/Program Files/LLVM/bin"
                "C:/Program Files (x86)/LLVM/bin"
        )
        find_program(MSVC_LIB_EXECUTABLE NAMES lib lib.exe)

        if(LLVM_LIB_EXECUTABLE)
            set(CMAKE_AR "${LLVM_LIB_EXECUTABLE}" CACHE FILEPATH "Archiver" FORCE)
            message(STATUS "Using LLVM archiver for clang-cl: ${LLVM_LIB_EXECUTABLE}")
        elseif(MSVC_LIB_EXECUTABLE)
            set(CMAKE_AR "${MSVC_LIB_EXECUTABLE}" CACHE FILEPATH "Archiver" FORCE)
            message(STATUS "Using MSVC archiver for clang-cl: ${MSVC_LIB_EXECUTABLE}")
        else()
            message(WARNING "No MSVC-compatible archiver found for clang-cl. Build may fail.")
        endif()
        set(CMAKE_RANLIB "" CACHE FILEPATH "Ranlib" FORCE)
    else()
        # On Unix-like systems with Clang, use llvm-ar and llvm-ranlib
        find_program(LLVM_AR_EXECUTABLE llvm-ar)
        find_program(LLVM_RANLIB_EXECUTABLE llvm-ranlib)
        if(LLVM_AR_EXECUTABLE)
            set(CMAKE_AR "${LLVM_AR_EXECUTABLE}" CACHE FILEPATH "Archiver" FORCE)
        endif()
        if(LLVM_RANLIB_EXECUTABLE)
            set(CMAKE_RANLIB "${LLVM_RANLIB_EXECUTABLE}" CACHE FILEPATH "Ranlib" FORCE)
        endif()
    endif()
endif()

# Only generate specific configs for multi-config generators
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo;Release" CACHE STRING "" FORCE)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEVELOPER_MODE "Enable developer-focused build features" ${PROJECT_IS_TOP_LEVEL})
if(NOT DEFINED BUILD_TESTING)
    set(BUILD_TESTING ${PROJECT_IS_TOP_LEVEL} CACHE BOOL "Build tests")
endif()
option(ROSCRAFT_CORE_ENABLE_UNITY_BUILD "Enable Unity/Jumbo builds" OFF)
option(ROSCRAFT_CORE_ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# Unity build configuration
if(ROSCRAFT_CORE_ENABLE_UNITY_BUILD)
    set(CMAKE_UNITY_BUILD ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 16 CACHE STRING "Unity build batch size")
endif()

# Set up directories
set(MODULE_NAME CORE)
set(ROSCRAFT_CORE_DIR ${PROJECT_SOURCE_DIR}) # core module dir
set(ROSCRAFT_WS_DIR ${PROJECT_SOURCE_DIR}/..) # roscraft_ws dir
set(ROSCRAFT_ROOT_DIR ${PROJECT_SOURCE_DIR}/../..) # roscraft dir (entire project's root)

list(APPEND CMAKE_MODULE_PATH "${ROSCRAFT_WS_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${ROSCRAFT_WS_DIR}/cmake/helpers")
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(BuildDirHelper)

roscraft_print_build_dir_info()

# LTO configuration
include(CheckIPOSupported)
check_ipo_supported(RESULT ROSCRAFT_CORE_IPO_SUPPORTED OUTPUT error)
if(ROSCRAFT_CORE_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    message(STATUS "Link-Time Optimization (LTO) enabled for Release and RelWithDebInfo builds")
else()
    message(STATUS "IPO/LTO not supported: ${error}")
    set(ROSCRAFT_CORE_ENABLE_LTO OFF)
endif()

# Position-Independent Code (PIC) configuration
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    message(STATUS "Position-Independent Code (PIC) enabled for multi-config generators")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    message(STATUS "Position-Independent Code (PIC) enabled for ${CMAKE_BUILD_TYPE} builds")
endif()

# Include sanitizer support
include(${ROSCRAFT_WS_DIR}/cmake/Sanitizers.cmake)
roscraft_print_sanitizer_status()

include(${ROSCRAFT_WS_DIR}/cmake/Dependencies.cmake)

# Configure core
set(ROSCRAFT_CORE_HEADERS
    include/roscraft/core/assert.hpp
)

set(ROSCRAFT_CORE_SOURCES
    src/assert.cpp
)

set(ROSCRAFT_CORE_ALL_FILES
    ${ROSCRAFT_CORE_HEADERS}
    ${ROSCRAFT_CORE_SOURCES}
)

add_library(roscraft_core STATIC ${ROSCRAFT_CORE_ALL_FILES})
add_library(roscraft::core ALIAS roscraft_core)

include(${ROSCRAFT_WS_DIR}/cmake/TargetUtils.cmake)

roscraft_target_set_cxx_standard(roscraft_core STANDARD 23)
roscraft_target_set_platform(roscraft_core)
roscraft_target_set_output_dirs(roscraft_core)
roscraft_target_set_folder(roscraft_core "roscraft/core")

roscraft_target_add_pch(roscraft_core ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.hpp)
roscraft_target_set_warnings(roscraft_core)
roscraft_target_set_optimization(roscraft_core)
roscraft_target_enable_lto(roscraft_core)
roscraft_target_enable_sanitizers(roscraft_core)

target_include_directories(roscraft_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(roscraft_core PUBLIC
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:ROSCRAFT_CORE_ENABLE_ASSERTS;ROSCRAFT_CORE_ENABLE_STACKTRACE>
)

roscraft_require_dependency(Boost)

# Platform-specific configurations for stacktrace
if(ROSCRAFT_USE_STL_STACKTRACE)
    message(status "roscraft_core: Using C++23 STL stacktrace")
else()
    # Using Boost stacktrace, configure platform-specific backends
    if(UNIX AND NOT APPLE)
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libbacktrace.a
            OUTPUT_VARIABLE BACKTRACE_LIBRARY_FROM_COMPILER
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        set(BACKTRACE_LIBRARY "")
        if(BACKTRACE_LIBRARY_FROM_COMPILER
            AND NOT BACKTRACE_LIBRARY_FROM_COMPILER STREQUAL "libbacktrace.a"
            AND EXISTS "${BACKTRACE_LIBRARY_FROM_COMPILER}")
            set(BACKTRACE_LIBRARY "${BACKTRACE_LIBRARY_FROM_COMPILER}")
        else()
            find_library(BACKTRACE_LIBRARY NAMES backtrace libbacktrace)
        endif()

        find_library(DL_LIBRARY NAMES dl)
        if(BACKTRACE_LIBRARY)
            target_link_libraries(roscraft_core PRIVATE ${BACKTRACE_LIBRARY})
            if(DL_LIBRARY)
                target_link_libraries(roscraft_core PRIVATE ${DL_LIBRARY})
            endif()
            if(TARGET helios::boost::stacktrace_backtrace)
                target_link_libraries(roscraft_core PRIVATE helios::boost::stacktrace_backtrace)
            endif()
            get_filename_component(BACKTRACE_INCLUDE_DIR "${BACKTRACE_LIBRARY}" DIRECTORY)
            get_filename_component(BACKTRACE_INCLUDE_DIR "${BACKTRACE_INCLUDE_DIR}/include" ABSOLUTE)
            if(EXISTS "${BACKTRACE_INCLUDE_DIR}/backtrace.h")
                target_include_directories(roscraft_core PRIVATE ${BACKTRACE_INCLUDE_DIR})
            endif()
            target_compile_definitions(roscraft_core PRIVATE BOOST_STACKTRACE_USE_BACKTRACE)
        else()
            if(TARGET helios::boost::stacktrace_addr2line)
                target_link_libraries(roscraft_core PRIVATE
                    helios::boost::stacktrace_addr2line
                )
            endif()
            if(DL_LIBRARY)
                target_link_libraries(roscraft_core PRIVATE ${DL_LIBRARY})
            endif()
            target_compile_definitions(roscraft_core PRIVATE
                BOOST_STACKTRACE_USE_ADDR2LINE
            )
            message(STATUS "roscraft_core: libbacktrace not found, using Boost addr2line backend")
        endif()
    elseif(APPLE)
        target_compile_definitions(roscraft_core PRIVATE BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)
    elseif(WIN32)
        target_compile_definitions(roscraft_core PRIVATE BOOST_STACKTRACE_USE_WINDBG)
    endif()
endif()

if(UNIX AND NOT APPLE)
    if(NOT ROSCRAFT_USE_STL_STACKTRACE)
        target_link_options(roscraft_core INTERFACE
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-rdynamic>
        )
    endif()
endif()

# Configure tests
if(BUILD_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print dependency summary after everything is configured
roscraft_print_dependency_summary()
