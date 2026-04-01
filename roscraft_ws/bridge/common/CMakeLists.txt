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
option(ROSCRAFT_BRIDGE_COMMON_ENABLE_ROS2 "Enable ROS 2 support for bridge/common" ON)
option(ROSCRAFT_BRIDGE_COMMON_ENABLE_JNI_BACKEND "Enable JNI backend support" ON)
option(ROSCRAFT_BRIDGE_COMMON_ENABLE_NETWORK_BACKEND "Enable network backend support" ON)

if(NOT ROSCRAFT_BRIDGE_COMMON_ENABLE_JNI_BACKEND AND NOT ROSCRAFT_BRIDGE_COMMON_ENABLE_NETWORK_BACKEND)
    message(FATAL_ERROR "At least one backend must be enabled (JNI and/or network)")
endif()

if(ROSCRAFT_BRIDGE_COMMON_ENABLE_UNITY_BUILD)
    set(CMAKE_UNITY_BUILD ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 16 CACHE STRING "Unity build batch size")
endif()

set(MODULE_NAME BRIDGE_COMMON)
set(ROSCRAFT_BRIDGE_COMMON_DIR ${PROJECT_SOURCE_DIR})
set(ROSCRAFT_WS_DIR ${PROJECT_SOURCE_DIR}/../..)
set(ROSCRAFT_ROOT_DIR ${PROJECT_SOURCE_DIR}/../../..)

list(APPEND CMAKE_MODULE_PATH "${ROSCRAFT_WS_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${ROSCRAFT_WS_DIR}/cmake/helpers")
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(BuildDirHelper)

roscraft_print_build_dir_info()

include(CheckIPOSupported)
check_ipo_supported(RESULT ROSCRAFT_BRIDGE_COMMON_IPO_SUPPORTED OUTPUT error)
if(ROSCRAFT_BRIDGE_COMMON_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    message(STATUS "Link-Time Optimization (LTO) enabled for Release and RelWithDebInfo builds")
else()
    message(STATUS "IPO/LTO not supported: ${error}")
endif()

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    message(STATUS "Position-Independent Code (PIC) enabled for multi-config generators")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    message(STATUS "Position-Independent Code (PIC) enabled for ${CMAKE_BUILD_TYPE} builds")
endif()

include(${ROSCRAFT_WS_DIR}/cmake/Sanitizers.cmake)
roscraft_print_sanitizer_status()

include(${ROSCRAFT_WS_DIR}/cmake/Dependencies.cmake)

if(ROSCRAFT_BRIDGE_COMMON_ENABLE_ROS2)
    find_package(ament_cmake REQUIRED)
    find_package(action_msgs REQUIRED)
    find_package(builtin_interfaces REQUIRED)
    find_package(geometry_msgs REQUIRED)
    find_package(rclcpp REQUIRED)
    find_package(rclcpp_action REQUIRED)
    find_package(rosidl_default_generators REQUIRED)
    find_package(std_msgs REQUIRED)
endif()

# Find roscraft_core — installed by core's package into the colcon workspace.
# The roscraft::core imported target becomes available after this call.
find_package(roscraft_core REQUIRED)

set(ROSCRAFT_BRIDGE_COMMON_HEADERS
    include/roscraft/bridge/common/assert.hpp
    include/roscraft/bridge/common/backend.hpp
    include/roscraft/bridge/common/bridge.hpp

    include/roscraft/bridge/common/command/command.hpp
    include/roscraft/bridge/common/command/queue.hpp

    include/roscraft/bridge/common/command/types/command_types.hpp
    include/roscraft/bridge/common/command/types/kinds.hpp
    include/roscraft/bridge/common/command/types/status.hpp
    include/roscraft/bridge/common/command/types/header.hpp
    include/roscraft/bridge/common/command/types/response.hpp
)

set(ROSCRAFT_BRIDGE_COMMON_SOURCES
    src/assert.cpp
)

add_library(roscraft_bridge_common STATIC
    ${ROSCRAFT_BRIDGE_COMMON_HEADERS}
    ${ROSCRAFT_BRIDGE_COMMON_SOURCES}
)
add_library(roscraft::bridge::common ALIAS roscraft_bridge_common)

include(${ROSCRAFT_WS_DIR}/cmake/TargetUtils.cmake)

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
    $<$<BOOL:${ROSCRAFT_BRIDGE_COMMON_ENABLE_ROS2}>:ROSCRAFT_BRIDGE_ENABLE_ROS2>
    $<$<BOOL:${ROSCRAFT_BRIDGE_COMMON_ENABLE_JNI_BACKEND}>:ROSCRAFT_BRIDGE_ENABLE_JNI_BACKEND>
    $<$<BOOL:${ROSCRAFT_BRIDGE_COMMON_ENABLE_NETWORK_BACKEND}>:ROSCRAFT_BRIDGE_ENABLE_NETWORK_BACKEND>
)

roscraft_require_dependency(readerwriterqueue)
roscraft_require_dependency(concurrentqueue)

target_link_libraries(roscraft_bridge_common PUBLIC
    roscraft::core
)

if(ROSCRAFT_BRIDGE_COMMON_ENABLE_ROS2)
    ament_target_dependencies(roscraft_bridge_common
        action_msgs
        builtin_interfaces
        geometry_msgs
        rclcpp
        rclcpp_action
        std_msgs
    )
endif()

set(ROSCRAFT_BRIDGE_COMMON_INTERFACE_FILES)
file(GLOB ROSCRAFT_BRIDGE_COMMON_MSG_FILES    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} msg/*.msg)
file(GLOB ROSCRAFT_BRIDGE_COMMON_SRV_FILES    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} srv/*.srv)
file(GLOB ROSCRAFT_BRIDGE_COMMON_ACTION_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} action/*.action)

list(APPEND ROSCRAFT_BRIDGE_COMMON_INTERFACE_FILES
    ${ROSCRAFT_BRIDGE_COMMON_MSG_FILES}
    ${ROSCRAFT_BRIDGE_COMMON_SRV_FILES}
    ${ROSCRAFT_BRIDGE_COMMON_ACTION_FILES}
)

if(ROSCRAFT_BRIDGE_COMMON_ENABLE_ROS2 AND ROSCRAFT_BRIDGE_COMMON_INTERFACE_FILES)
    rosidl_generate_interfaces(${PROJECT_NAME}
        ${ROSCRAFT_BRIDGE_COMMON_INTERFACE_FILES}
        DEPENDENCIES
            action_msgs
            builtin_interfaces
            geometry_msgs
            std_msgs
        ADD_LINTER_TESTS
    )

    rosidl_get_typesupport_target(ROSCRAFT_BRIDGE_COMMON_CPP_TYPESUPPORT_TARGET
        ${PROJECT_NAME}
        rosidl_typesupport_cpp
    )
    target_link_libraries(roscraft_bridge_common PRIVATE
        ${ROSCRAFT_BRIDGE_COMMON_CPP_TYPESUPPORT_TARGET}
    )
endif()

install(TARGETS roscraft_bridge_common
    EXPORT export_roscraft_bridge_common
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)

if(BUILD_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ROSCRAFT_BRIDGE_COMMON_ENABLE_ROS2)
    ament_export_include_directories(include)
    ament_export_targets(export_roscraft_bridge_common HAS_LIBRARY_TARGET)
    ament_export_dependencies(
        roscraft_core           # propagate so jni/network don't need to re-find it
        action_msgs
        builtin_interfaces
        geometry_msgs
        rclcpp
        rclcpp_action
        std_msgs
    )

    if(ROSCRAFT_BRIDGE_COMMON_INTERFACE_FILES)
        ament_export_dependencies(rosidl_default_runtime)
    endif()

    ament_package()
endif()

roscraft_print_dependency_summary()
