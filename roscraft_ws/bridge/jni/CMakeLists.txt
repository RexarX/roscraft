cmake_minimum_required(VERSION 3.25)

project(roscraft_bridge_jni
    VERSION 0.1.0
    DESCRIPTION "Roscraft bridge jni module"
    HOMEPAGE_URL "https://github.com/RexarX/roscraft"
    LANGUAGES CXX
)

# Set archiver tools for Clang to support LTO
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
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

if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo;Release" CACHE STRING "" FORCE)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEVELOPER_MODE "Enable developer-focused build features" OFF)
option(ROSCRAFT_BRIDGE_JNI_BUILD_TESTS "Build roscraft bridge jni tests" OFF)
if(NOT DEFINED BUILD_TESTING)
  set(BUILD_TESTING ${ROSCRAFT_BRIDGE_JNI_BUILD_TESTS} CACHE BOOL "Build tests")
endif()
option(ROSCRAFT_BRIDGE_JNI_ENABLE_UNITY_BUILD "Enable Unity/Jumbo builds" OFF)
option(ROSCRAFT_BRIDGE_ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(ROSCRAFT_BRIDGE_JNI_ENABLE_UNITY_BUILD)
  set(CMAKE_UNITY_BUILD ON)
  set(CMAKE_UNITY_BUILD_BATCH_SIZE 16 CACHE STRING "Unity build batch size")
endif()

set(MODULE_NAME BRIDGE_JNI)
set(ROSCRAFT_BRIDGE_JNI_DIR ${PROJECT_SOURCE_DIR})
set(ROSCRAFT_BRIDGE_DIR ${PROJECT_SOURCE_DIR}/..)
set(ROSCRAFT_WS_DIR ${PROJECT_SOURCE_DIR}/../..)
set(ROSCRAFT_ROOT_DIR ${PROJECT_SOURCE_DIR}/../../..)

list(APPEND CMAKE_MODULE_PATH "${ROSCRAFT_WS_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${ROSCRAFT_WS_DIR}/cmake/helpers")
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(BuildDirHelper)

roscraft_print_build_dir_info()

include(CheckIPOSupported)
check_ipo_supported(RESULT ROSCRAFT_BRIDGE_IPO_SUPPORTED OUTPUT error)
if(ROSCRAFT_BRIDGE_IPO_SUPPORTED)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  message(STATUS "Link-Time Optimization (LTO) enabled for Release and RelWithDebInfo builds")
else()
  message(STATUS "IPO/LTO not supported: ${error}")
endif()

if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  message(STATUS "Position-Independent Code (PIC) enabled for multi-config generators")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  message(STATUS "Position-Independent Code (PIC) enabled for ${CMAKE_BUILD_TYPE} builds")
endif()

include(${ROSCRAFT_WS_DIR}/cmake/Sanitizers.cmake)
include(${ROSCRAFT_WS_DIR}/cmake/Dependencies.cmake)
include(${ROSCRAFT_WS_DIR}/cmake/TargetUtils.cmake)

roscraft_print_sanitizer_status()

roscraft_require_dependency(FlatBuffers)

find_package(roscraft_bridge_common REQUIRED)
if(TARGET roscraft::roscraft_bridge_common AND NOT TARGET roscraft::bridge::common)
  add_library(roscraft::bridge::common ALIAS roscraft::roscraft_bridge_common)
endif()
if(TARGET roscraft_bridge_common::roscraft_bridge_common AND NOT TARGET roscraft::bridge::common)
  add_library(roscraft::bridge::common ALIAS roscraft_bridge_common::roscraft_bridge_common)
endif()
if(TARGET roscraft_bridge_common::roscraft_bridge_common_lib AND NOT TARGET roscraft::bridge::common)
  add_library(roscraft::bridge::common ALIAS roscraft_bridge_common::roscraft_bridge_common_lib)
endif()
if(TARGET roscraft::roscraft_bridge_fbs AND NOT TARGET roscraft::bridge::fbs)
  add_library(roscraft::bridge::fbs ALIAS roscraft::roscraft_bridge_fbs)
endif()
if(TARGET roscraft_bridge_common::roscraft_bridge_fbs AND NOT TARGET roscraft::bridge::fbs)
  add_library(roscraft::bridge::fbs ALIAS roscraft_bridge_common::roscraft_bridge_fbs)
endif()

find_package(roscraft_core REQUIRED)
if(TARGET roscraft::roscraft_core AND NOT TARGET roscraft::core)
  add_library(roscraft::core ALIAS roscraft::roscraft_core)
endif()
if(TARGET roscraft_core::roscraft_core AND NOT TARGET roscraft::core)
  add_library(roscraft::core ALIAS roscraft_core::roscraft_core)
endif()

find_package(ament_cmake REQUIRED)

find_package(JNI REQUIRED)

set(ROSCRAFT_BRIDGE_JNI_HEADERS
    include/roscraft/bridge/jni/bridge.hpp
    include/roscraft/bridge/jni/config.hpp
    include/roscraft/bridge/jni/env_guard.hpp
    include/roscraft/bridge/jni/env_setup.hpp
    include/roscraft/bridge/jni/native_methods.hpp

    include/roscraft/bridge/jni/command/callback.hpp
    include/roscraft/bridge/jni/command/jni_sink.hpp
)

set(ROSCRAFT_BRIDGE_JNI_SOURCES
    src/bridge.cpp
    src/env_guard.cpp
    src/env_setup.cpp
    src/native_methods.cpp
)

set(ROSCRAFT_BRIDGE_JNI_ALL_FILES
    ${ROSCRAFT_BRIDGE_JNI_HEADERS}
    ${ROSCRAFT_BRIDGE_JNI_SOURCES}
)

add_library(roscraft_bridge_jni SHARED ${ROSCRAFT_BRIDGE_JNI_ALL_FILES})
add_library(roscraft::bridge::jni ALIAS roscraft_bridge_jni)

roscraft_target_set_cxx_standard(roscraft_bridge_jni STANDARD 23)
roscraft_target_set_platform(roscraft_bridge_jni)
roscraft_target_set_shared_lib_platform(roscraft_bridge_jni)
roscraft_target_set_output_dirs(roscraft_bridge_jni)
roscraft_target_set_folder(roscraft_bridge_jni "roscraft/bridge/jni")

roscraft_target_add_pch(roscraft_bridge_jni ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.hpp)
roscraft_target_set_warnings(roscraft_bridge_jni)
roscraft_target_set_optimization(roscraft_bridge_jni)
roscraft_target_enable_lto(roscraft_bridge_jni)
roscraft_target_enable_sanitizers(roscraft_bridge_jni)

target_include_directories(roscraft_bridge_jni
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(roscraft_bridge_jni SYSTEM PRIVATE
    ${JNI_INCLUDE_DIRS}
)

target_compile_definitions(roscraft_bridge_jni PRIVATE
    ROSCRAFT_BRIDGE_JNI_BUILD_SHARED
)

# Derive the ROS library directory from rclcpp's CMake config path
# so the JNI .so can find ROS shared libraries at runtime without
# requiring LD_LIBRARY_PATH to be set. rclcpp_DIR is set by
# find_package(rclcpp REQUIRED) above; it points to
#   <prefix>/share/rclcpp/cmake/
# ROS libraries are at <prefix>/lib/.
get_filename_component(_rclcpp_subshare_dir "${rclcpp_DIR}/../.." REALPATH)
get_filename_component(_ros_lib_dir "${_rclcpp_subshare_dir}/../lib" REALPATH)
message(STATUS "Adding ROS library path to JNI RPATH: ${_ros_lib_dir}")

set_target_properties(roscraft_bridge_jni PROPERTIES
    INSTALL_RPATH "$ORIGIN:${_ros_lib_dir}"
    BUILD_RPATH "$ORIGIN:${_ros_lib_dir}"
)

if(TARGET JNI::JNI)
  target_link_libraries(roscraft_bridge_jni PRIVATE JNI::JNI)
else()
  target_link_libraries(roscraft_bridge_jni PRIVATE ${JNI_LIBRARIES})
endif()

roscraft_require_dependency(FlatMap)

target_link_libraries(roscraft_bridge_jni
  PUBLIC
      roscraft::bridge::common
      roscraft::bridge::fbs
      roscraft::core
      roscraft::flat_map
  PRIVATE
      rclcpp::rclcpp
      ${CMAKE_DL_LIBS}
)

install(TARGETS roscraft_bridge_jni
    EXPORT export_roscraft_bridge_jni
    LIBRARY DESTINATION lib
)

if(BUILD_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

install(DIRECTORY include/
    DESTINATION include
)

ament_export_dependencies(
    roscraft_bridge_common
    roscraft_core
    rclcpp
)
ament_export_include_directories(include)
ament_export_targets(export_roscraft_bridge_jni HAS_LIBRARY_TARGET)
ament_package()

roscraft_print_dependency_summary()
