cmake_minimum_required(VERSION 3.25)

project(roscraft_bridge_common
    VERSION 0.1.0
    DESCRIPTION "Roscraft bridge common module"
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

option(DEVELOPER_MODE "Enable developer-focused build features" ${PROJECT_IS_TOP_LEVEL})
if(NOT DEFINED BUILD_TESTING)
  set(BUILD_TESTING ${PROJECT_IS_TOP_LEVEL} CACHE BOOL "Build tests")
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

# Find roscraft_core — installed by core's package into the colcon workspace.
# The roscraft::core imported target becomes available after this call.
find_package(roscraft_core REQUIRED)

if(TARGET roscraft::roscraft_core AND NOT TARGET roscraft::core)
  add_library(roscraft::core ALIAS roscraft::roscraft_core)
endif()

find_package(ament_cmake REQUIRED)
find_package(action_msgs REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
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

    include/roscraft/bridge/nodes/graph.hpp
    include/roscraft/bridge/nodes/topic_relay.hpp
)

set(ROSCRAFT_BRIDGE_COMMON_SOURCES
    src/assert.cpp

    src/app/app.cpp

    src/nodes/graph.cpp
    src/nodes/topic_relay.cpp
)

add_library(roscraft_bridge_common STATIC
    ${ROSCRAFT_BRIDGE_COMMON_HEADERS}
    ${ROSCRAFT_BRIDGE_COMMON_SOURCES}
)
add_library(roscraft::bridge::common ALIAS roscraft_bridge_common)

roscraft_target_set_cxx_standard(roscraft_bridge_common STANDARD 23)
roscraft_target_set_platform(roscraft_bridge_common)
roscraft_target_set_output_dirs(roscraft_bridge_common)
roscraft_target_set_folder(roscraft_bridge_common "roscraft/bridge/common")

roscraft_target_add_pch(roscraft_bridge_common ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.hpp)
roscraft_target_set_warnings(roscraft_bridge_common)
roscraft_target_set_optimization(roscraft_bridge_common)
roscraft_target_enable_lto(roscraft_bridge_common)
roscraft_target_enable_sanitizers(roscraft_bridge_common)

target_include_directories(roscraft_bridge_common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(roscraft_bridge_common PUBLIC
    $<$<BOOL:${ROSCRAFT_BRIDGE_ENABLE_JNI_BACKEND}>:ROSCRAFT_BRIDGE_ENABLE_JNI_BACKEND>
    $<$<BOOL:${ROSCRAFT_BRIDGE_ENABLE_NETWORK_BACKEND}>:ROSCRAFT_BRIDGE_ENABLE_NETWORK_BACKEND>
)

roscraft_require_dependency(concurrentqueue)
roscraft_require_dependency(Taskflow)

target_link_libraries(roscraft_bridge_common
    PUBLIC
        roscraft::core
        rclcpp::rclcpp
)

if(TARGET roscraft::flat_map)
  target_link_libraries(roscraft_bridge_common PUBLIC
        $<BUILD_INTERFACE:roscraft::flat_map>
        $<INSTALL_INTERFACE:roscraft::flat_map>
    )
  if(roscraft_USE_STL_FLAT_MAP)
    target_compile_definitions(roscraft_bridge_common PUBLIC
            $<INSTALL_INTERFACE:roscraft_USE_STL_FLAT_MAP>
        )
  endif()
endif()

if(TARGET concurrentqueue::concurrentqueue OR TARGET concurrentqueue)
  target_link_libraries(roscraft_bridge_common PUBLIC
        $<BUILD_INTERFACE:roscraft::concurrentqueue>
        $<INSTALL_INTERFACE:concurrentqueue::concurrentqueue>
    )
endif()

if(TARGET Taskflow::Taskflow OR TARGET Taskflow)
  target_link_libraries(roscraft_bridge_common PUBLIC
        $<BUILD_INTERFACE:roscraft::taskflow::taskflow>
        $<INSTALL_INTERFACE:Taskflow::Taskflow>
    )
endif()

# ─────────────────────────────────────────────────────────────────────────────
# FlatBuffers code generation
# ─────────────────────────────────────────────────────────────────────────────
set(ROSCRAFT_FBS_SCHEMAS
    ${ROSCRAFT_ROOT_DIR}/schemas/bridge_packets.fbs
)

set(FLATC_FLAGS
    --cpp
    --cpp-std "c++17"
    --gen-object-api
    --scoped-enums
    --filename-ext "hpp"
)

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
  find_program(FLATC_EXECUTABLE flatc REQUIRED)
endif()

set(ROSCRAFT_FBS_GENERATED_HEADERS "")
foreach(_schema ${ROSCRAFT_FBS_SCHEMAS})
  get_filename_component(_schema_name "${_schema}" NAME_WE)
  set(_generated_header ${ROSCRAFT_GENERATED_DIR}/include/roscraft/generated/${_schema_name}_generated.hpp)
  set(_output_dir ${ROSCRAFT_GENERATED_DIR}/include/roscraft/generated)

  add_custom_command(
        OUTPUT  "${_generated_header}"
        COMMAND "${FLATC_EXECUTABLE}"
                ${FLATC_FLAGS}
                -o "${_output_dir}"
                "${_schema}"
        DEPENDS "${_schema}"
        COMMENT "Compiling Flatbuffers schema: ${_schema_name}.fbs → ${_schema_name}.hpp"
        VERBATIM
  )

  list(APPEND ROSCRAFT_FBS_GENERATED_HEADERS "${_generated_header}")
endforeach()

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

if(TARGET _roscraft_flatbuffers_all)
  install(TARGETS roscraft_bridge_fbs _roscraft_flatbuffers_all
      EXPORT export_roscraft_bridge_common
      FILE_SET HEADERS DESTINATION include
  )
else()
  install(TARGETS roscraft_bridge_fbs
      EXPORT export_roscraft_bridge_common
      FILE_SET HEADERS DESTINATION include
  )
endif()

add_custom_target(roscraft_bridge_fbs_generate ALL
    DEPENDS ${ROSCRAFT_FBS_GENERATED_HEADERS}
    COMMENT "Flatbuffers code generation"
)

add_dependencies(roscraft_bridge_fbs roscraft_bridge_fbs_generate)

target_link_libraries(roscraft_bridge_common PUBLIC roscraft::bridge::fbs)

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
        ADD_LINTER_TESTS
    )

  rosidl_get_typesupport_target(ROSCRAFT_BRIDGE_COMMON_CPP_TYPESUPPORT_TARGET
        roscraft_bridge_common
        rosidl_typesupport_cpp
    )
  target_link_libraries(roscraft_bridge_common PRIVATE
        ${ROSCRAFT_BRIDGE_COMMON_CPP_TYPESUPPORT_TARGET}
    )
endif()

install(TARGETS roscraft_bridge_common
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
    roscraft_core
    concurrentqueue
    Taskflow
    action_msgs
    builtin_interfaces
    geometry_msgs
    rclcpp
    std_msgs
)
ament_export_include_directories(include)
ament_export_targets(export_roscraft_bridge_common HAS_LIBRARY_TARGET)
if(ROSCRAFT_BRIDGE_COMMON_INTERFACE_FILES)
  ament_export_dependencies(rosidl_default_runtime)
endif()
ament_package()

roscraft_print_dependency_summary()
