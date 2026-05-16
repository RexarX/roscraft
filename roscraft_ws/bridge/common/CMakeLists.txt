cmake_minimum_required(VERSION 3.25)

project(roscraft_bridge_common
    VERSION 0.1.0
    DESCRIPTION "Roscraft bridge common module"
    HOMEPAGE_URL "https://github.com/RexarX/roscraft"
    LANGUAGES CXX C
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
option(ROSCRAFT_BRIDGE_COMMON_BUILD_TESTS "Build roscraft bridge common tests" OFF)
if(NOT DEFINED BUILD_TESTING)
  set(BUILD_TESTING ${ROSCRAFT_BRIDGE_COMMON_BUILD_TESTS} CACHE BOOL "Build tests")
endif()
option(ROSCRAFT_BRIDGE_COMMON_ENABLE_UNITY_BUILD "Enable Unity/Jumbo builds" OFF)
option(ROSCRAFT_BRIDGE_COMMON_ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(ROSCRAFT_BRIDGE_COMMON_ENABLE_UNITY_BUILD)
  set(CMAKE_UNITY_BUILD ON)
  set(CMAKE_UNITY_BUILD_BATCH_SIZE 16 CACHE STRING "Unity build batch size")
endif()

set(MODULE_NAME BRIDGE_COMMON)
set(ROSCRAFT_BRIDGE_COMMON_DIR ${PROJECT_SOURCE_DIR})
set(ROSCRAFT_BRIDGE_DIR ${PROJECT_SOURCE_DIR}/..)
set(ROSCRAFT_WS_DIR ${PROJECT_SOURCE_DIR}/../..)
set(ROSCRAFT_ROOT_DIR ${PROJECT_SOURCE_DIR}/../../..)
set(ROSCRAFT_GENERATED_DIR ${ROSCRAFT_WS_DIR}/generated)

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

roscraft_require_dependency(FlatMap)
roscraft_require_dependency(FlatBuffers)
roscraft_require_dependency(glaze)

# Find roscraft_core — installed by core's package into the colcon workspace.
# The roscraft::core imported target becomes available after this call.
find_package(roscraft_core REQUIRED)

if(TARGET roscraft::roscraft_core AND NOT TARGET roscraft::core)
  add_library(roscraft::core ALIAS roscraft::roscraft_core)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(action_msgs REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_srvs REQUIRED)
find_package(std_msgs REQUIRED)

# common bridge configuration
set(ROSCRAFT_BRIDGE_COMMON_HEADERS
    include/roscraft/bridge/assert.hpp
    include/roscraft/bridge/bridge.hpp

    include/roscraft/bridge/app/app.hpp
    include/roscraft/bridge/app/config.hpp

    include/roscraft/bridge/command/command.hpp
    include/roscraft/bridge/command/commands.hpp
    include/roscraft/bridge/command/queue.hpp

    include/roscraft/bridge/command/types/action.hpp
    include/roscraft/bridge/command/types/addon.hpp
    include/roscraft/bridge/command/types/common.hpp
    include/roscraft/bridge/command/types/error.hpp
    include/roscraft/bridge/command/types/graph.hpp
    include/roscraft/bridge/command/types/interface.hpp
    include/roscraft/bridge/command/types/node.hpp
    include/roscraft/bridge/command/types/param.hpp
    include/roscraft/bridge/command/types/player.hpp
    include/roscraft/bridge/command/types/service.hpp
    include/roscraft/bridge/command/types/topic.hpp

    include/roscraft/bridge/nodes/ros/common.hpp
    include/roscraft/bridge/nodes/ros/nodes.hpp

    include/roscraft/bridge/nodes/ros/action/info.hpp
    include/roscraft/bridge/nodes/ros/action/send_goal.hpp

    include/roscraft/bridge/nodes/addon/event.hpp

    include/roscraft/bridge/nodes/ros/graph/cache.hpp

    include/roscraft/bridge/nodes/ros/interface/list.hpp
    include/roscraft/bridge/nodes/ros/interface/show.hpp

    include/roscraft/bridge/nodes/ros/node/info.hpp

    include/roscraft/bridge/nodes/ros/param/describe.hpp
    include/roscraft/bridge/nodes/ros/param/dump.hpp
    include/roscraft/bridge/nodes/ros/param/get.hpp
    include/roscraft/bridge/nodes/ros/param/list.hpp
    include/roscraft/bridge/nodes/ros/param/load.hpp
    include/roscraft/bridge/nodes/ros/param/set.hpp

    include/roscraft/bridge/nodes/ros/service/call.hpp
    include/roscraft/bridge/nodes/ros/service/info.hpp

    include/roscraft/bridge/nodes/ros/topic/info.hpp
    include/roscraft/bridge/nodes/ros/topic/relay.hpp
    include/roscraft/bridge/nodes/ros/topic/stats.hpp

    include/roscraft/bridge/nodes/ros/details/graph_utils.hpp
    include/roscraft/bridge/nodes/ros/details/introspection_codec.hpp
)

set(ROSCRAFT_BRIDGE_COMMON_SOURCES
    src/assert.cpp

    src/app/app.cpp

    src/nodes/ros/action/info.cpp
    src/nodes/ros/action/send_goal.cpp

    src/nodes/addon/event.cpp

    src/nodes/ros/graph/cache.cpp

    src/nodes/ros/interface/list.cpp
    src/nodes/ros/interface/show.cpp

    src/nodes/ros/node/info.cpp

    src/nodes/ros/param/describe.cpp
    src/nodes/ros/param/dump.cpp
    src/nodes/ros/param/get.cpp
    src/nodes/ros/param/list.cpp
    src/nodes/ros/param/load.cpp
    src/nodes/ros/param/set.cpp

    src/nodes/ros/service/call.cpp
    src/nodes/ros/service/info.cpp

    src/nodes/ros/topic/info.cpp
    src/nodes/ros/topic/relay.cpp
    src/nodes/ros/topic/stats.cpp

    src/nodes/ros/details/graph_utils.cpp
    src/nodes/ros/details/introspection_codec.cpp
)

add_library(roscraft_bridge_common_lib STATIC
    ${ROSCRAFT_BRIDGE_COMMON_HEADERS}
    ${ROSCRAFT_BRIDGE_COMMON_SOURCES}
)
add_library(roscraft::bridge::common ALIAS roscraft_bridge_common_lib)

roscraft_target_set_cxx_standard(roscraft_bridge_common_lib STANDARD 23)
roscraft_target_set_platform(roscraft_bridge_common_lib)
roscraft_target_set_output_dirs(roscraft_bridge_common_lib)
roscraft_target_set_folder(roscraft_bridge_common_lib "roscraft/bridge/common")

roscraft_target_add_pch(roscraft_bridge_common_lib ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.hpp)
roscraft_target_set_warnings(roscraft_bridge_common_lib)
roscraft_target_set_optimization(roscraft_bridge_common_lib)
roscraft_target_enable_lto(roscraft_bridge_common_lib)
roscraft_target_enable_sanitizers(roscraft_bridge_common_lib)

target_include_directories(roscraft_bridge_common_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(roscraft_bridge_common_lib PUBLIC
    $<$<BOOL:${ROSCRAFT_BRIDGE_ENABLE_JNI_BACKEND}>:ROSCRAFT_BRIDGE_ENABLE_JNI_BACKEND>
    $<$<BOOL:${ROSCRAFT_BRIDGE_ENABLE_NETWORK_BACKEND}>:ROSCRAFT_BRIDGE_ENABLE_NETWORK_BACKEND>
)

roscraft_require_dependency(concurrentqueue)

target_link_libraries(roscraft_bridge_common_lib
    PUBLIC
        roscraft::core
        rclcpp::rclcpp
    PRIVATE
      $<BUILD_INTERFACE:roscraft::glaze::glaze>
      ament_index_cpp::ament_index_cpp
      action_msgs::action_msgs__rosidl_typesupport_cpp
      geometry_msgs::geometry_msgs__rosidl_typesupport_cpp
      std_srvs::std_srvs__rosidl_typesupport_cpp
      std_msgs::std_msgs__rosidl_typesupport_cpp
      unique_identifier_msgs::unique_identifier_msgs__rosidl_typesupport_cpp
)

if(TARGET roscraft::flat_map)
  target_link_libraries(roscraft_bridge_common_lib PUBLIC
      $<BUILD_INTERFACE:roscraft::flat_map>
      $<INSTALL_INTERFACE:roscraft::flat_map>
  )
  if(roscraft_USE_STL_FLAT_MAP)
    target_compile_definitions(roscraft_bridge_common_lib PUBLIC
        $<INSTALL_INTERFACE:roscraft_USE_STL_FLAT_MAP>
    )
  endif()
endif()

if(TARGET concurrentqueue::concurrentqueue OR TARGET concurrentqueue)
  target_link_libraries(roscraft_bridge_common_lib PUBLIC
      $<BUILD_INTERFACE:roscraft::concurrentqueue>
      $<INSTALL_INTERFACE:concurrentqueue::concurrentqueue>
  )
endif()

# ─────────────────────────────────────────────────────────────────────────────
# FlatBuffers code generation
# ─────────────────────────────────────────────────────────────────────────────
set(ROSCRAFT_FBS_SCHEMAS
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets.fbs
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets/common.fbs
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets/graph.fbs
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets/node.fbs
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets/topic.fbs
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets/service.fbs
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets/interface.fbs
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets/param.fbs
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets/action.fbs
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets/player.fbs
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets/error.fbs
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets/addon.fbs
)

set(FLATC_FLAGS
    --cpp
    --cpp-std "c++17"
    --gen-object-api
    --scoped-enums
    --filename-ext "hpp"
)

set(FLATC_EXECUTABLE "")
if(TARGET flatbuffers::flatc)
  get_target_property(FLATC_EXECUTABLE flatbuffers::flatc IMPORTED_LOCATION)
  if(NOT FLATC_EXECUTABLE)
    get_target_property(_flatc_configs flatbuffers::flatc IMPORTED_CONFIGURATIONS)
    foreach(_config IN LISTS _flatc_configs)
      get_target_property(FLATC_EXECUTABLE flatbuffers::flatc IMPORTED_LOCATION_${_config})
      if(FLATC_EXECUTABLE)
        break()
      endif()
    endforeach()
  endif()
  if(NOT FLATC_EXECUTABLE)
    get_target_property(FLATC_EXECUTABLE flatbuffers::flatc IMPORTED_LOCATION_NOCONFIG)
  endif()
endif()
if(NOT FLATC_EXECUTABLE)
  if(TARGET flatc)
    set(FLATC_EXECUTABLE "$<TARGET_FILE:flatc>")
  endif()
endif()
if(NOT FLATC_EXECUTABLE)
  if(EXISTS "${ROSCRAFT_WS_DIR}/install/roscraft_bridge_common/bin/flatc")
    set(FLATC_EXECUTABLE "${ROSCRAFT_WS_DIR}/install/roscraft_bridge_common/bin/flatc")
  endif()
endif()
if(NOT FLATC_EXECUTABLE)
  find_program(FLATC_EXECUTABLE flatc REQUIRED)
endif()

set(ROSCRAFT_FBS_GENERATED_HEADER_DIR
    ${ROSCRAFT_GENERATED_DIR}/include/roscraft/generated
)
set(ROSCRAFT_FBS_GENERATED_HEADER
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/bridge_packets.hpp
)
set(ROSCRAFT_FBS_GENERATED_HEADER_WITH_SUFFIX
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/bridge_packets_generated.hpp
)
set(ROSCRAFT_FBS_GENERATED_AUX_HEADERS
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/action_generated.hpp
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/addon_generated.hpp
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/common_generated.hpp
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/error_generated.hpp
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/graph_generated.hpp
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/interface_generated.hpp
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/node_generated.hpp
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/param_generated.hpp
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/player_generated.hpp
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/service_generated.hpp
    ${ROSCRAFT_FBS_GENERATED_HEADER_DIR}/topic_generated.hpp
)
set(ROSCRAFT_FBS_GENERATED_OUTPUTS
    ${ROSCRAFT_FBS_GENERATED_HEADER}
    ${ROSCRAFT_FBS_GENERATED_HEADER_WITH_SUFFIX}
    ${ROSCRAFT_FBS_GENERATED_AUX_HEADERS}
)
set(ROSCRAFT_FBS_GENERATED_HEADERS
    ${ROSCRAFT_FBS_GENERATED_HEADER}
    ${ROSCRAFT_FBS_GENERATED_AUX_HEADERS}
)

set(_flatc_depends ${ROSCRAFT_FBS_SCHEMAS})
if(TARGET flatbuffers::flatc)
  list(APPEND _flatc_depends flatbuffers::flatc)
elseif(TARGET flatc)
  list(APPEND _flatc_depends flatc)
endif()

add_custom_command(
    OUTPUT ${ROSCRAFT_FBS_GENERATED_OUTPUTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROSCRAFT_FBS_GENERATED_HEADER_DIR}"
    COMMAND "${FLATC_EXECUTABLE}"
            ${FLATC_FLAGS}
            -I "${ROSCRAFT_ROOT_DIR}/schemas"
            -o "${ROSCRAFT_FBS_GENERATED_HEADER_DIR}"
            ${ROSCRAFT_FBS_SCHEMAS}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ROSCRAFT_FBS_GENERATED_HEADER_WITH_SUFFIX}"
            "${ROSCRAFT_FBS_GENERATED_HEADER}"
    DEPENDS ${_flatc_depends}
    COMMENT "Compiling Flatbuffers schemas"
    VERBATIM
)

add_library(roscraft_bridge_fbs INTERFACE)
add_library(roscraft::bridge::fbs ALIAS roscraft_bridge_fbs)

target_sources(roscraft_bridge_fbs INTERFACE
    FILE_SET HEADERS
        BASE_DIRS ${ROSCRAFT_GENERATED_DIR}/include
        FILES ${ROSCRAFT_FBS_GENERATED_HEADERS}
)

target_include_directories(roscraft_bridge_fbs INTERFACE
    $<BUILD_INTERFACE:${ROSCRAFT_GENERATED_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(roscraft_bridge_fbs INTERFACE
    roscraft::flatbuffers
)

install(TARGETS roscraft_bridge_fbs
    EXPORT export_roscraft_bridge_common
    FILE_SET HEADERS DESTINATION include
)

add_custom_target(roscraft_bridge_fbs_generate ALL
    DEPENDS ${ROSCRAFT_FBS_GENERATED_HEADERS}
    COMMENT "Flatbuffers code generation"
)

add_dependencies(roscraft_bridge_fbs roscraft_bridge_fbs_generate)

target_link_libraries(roscraft_bridge_common_lib PUBLIC roscraft::bridge::fbs)

set(ROSCRAFT_BRIDGE_COMMON_INTERFACE_FILES)
file(GLOB ROSCRAFT_BRIDGE_COMMON_MSG_FILES    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} msg/*.msg)
file(GLOB ROSCRAFT_BRIDGE_COMMON_SRV_FILES    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} srv/*.srv)
file(GLOB ROSCRAFT_BRIDGE_COMMON_ACTION_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} action/*.action)

list(APPEND ROSCRAFT_BRIDGE_COMMON_INTERFACE_FILES
    ${ROSCRAFT_BRIDGE_COMMON_MSG_FILES}
    ${ROSCRAFT_BRIDGE_COMMON_SRV_FILES}
    ${ROSCRAFT_BRIDGE_COMMON_ACTION_FILES}
)

if(ROSCRAFT_BRIDGE_COMMON_INTERFACE_FILES)
  rosidl_generate_interfaces(roscraft_bridge_common
      ${ROSCRAFT_BRIDGE_COMMON_INTERFACE_FILES}
      DEPENDENCIES
          action_msgs
          builtin_interfaces
          geometry_msgs
          std_msgs
  )

  rosidl_get_typesupport_target(ROSCRAFT_BRIDGE_COMMON_CPP_TYPESUPPORT_TARGET
      roscraft_bridge_common
      rosidl_typesupport_cpp
  )
  target_link_libraries(roscraft_bridge_common_lib PUBLIC
      ${ROSCRAFT_BRIDGE_COMMON_CPP_TYPESUPPORT_TARGET}
  )
endif()

install(TARGETS roscraft_bridge_common_lib
    EXPORT export_roscraft_bridge_common
    ARCHIVE DESTINATION lib
)

install(EXPORT export_roscraft_bridge_common
    FILE roscraft_bridge_commonTargets.cmake
    NAMESPACE roscraft_bridge_common::
    DESTINATION lib/cmake/roscraft_bridge_common
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/roscraft_bridge_commonConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/roscraft_bridge_commonConfig.cmake
    INSTALL_DESTINATION lib/cmake/roscraft_bridge_common
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/roscraft_bridge_commonConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/roscraft_bridge_commonConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/roscraft_bridge_commonConfigVersion.cmake
    DESTINATION lib/cmake/roscraft_bridge_common
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
    ament_index_cpp
    roscraft_core
    concurrentqueue
    Taskflow
    action_msgs
    builtin_interfaces
    geometry_msgs
    rclcpp
    rclcpp_action
    std_srvs
    std_msgs
)
ament_export_include_directories(include)
ament_export_targets(export_roscraft_bridge_common HAS_LIBRARY_TARGET)
if(ROSCRAFT_BRIDGE_COMMON_INTERFACE_FILES)
  ament_export_dependencies(rosidl_default_runtime)
endif()
ament_package()

roscraft_print_dependency_summary()
