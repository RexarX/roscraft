cmake_minimum_required(VERSION 3.25)

project(roscraft_bridge_network
    VERSION 0.1.0
    DESCRIPTION "Roscraft bridge network module"
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
option(ROSCRAFT_BRIDGE_NETWORK_ENABLE_UNITY_BUILD "Enable Unity/Jumbo builds" OFF)
option(ROSCRAFT_BRIDGE_NETWORK_ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(ROSCRAFT_BRIDGE_NETWORK_ENABLE_ROS2 "Enable ROS 2 support for bridge/network" ON)

# Unity build configuration
if(ROSCRAFT_BRIDGE_NETWORK_ENABLE_UNITY_BUILD)
    set(CMAKE_UNITY_BUILD ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 16 CACHE STRING "Unity build batch size")
endif()

# Set up directories
set(MODULE_NAME BRIDGE_NETWORK)
set(ROSCRAFT_BRIDGE_NETWORK_DIR ${PROJECT_SOURCE_DIR})
set(ROSCRAFT_WS_DIR ${PROJECT_SOURCE_DIR}/../..)
set(ROSCRAFT_ROOT_DIR ${PROJECT_SOURCE_DIR}/../../..)

list(APPEND CMAKE_MODULE_PATH "${ROSCRAFT_WS_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${ROSCRAFT_WS_DIR}/cmake/helpers")
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(BuildDirHelper)

roscraft_print_build_dir_info()

# LTO configuration
include(CheckIPOSupported)
check_ipo_supported(RESULT ROSCRAFT_BRIDGE_NETWORK_IPO_SUPPORTED OUTPUT error)
if(ROSCRAFT_BRIDGE_NETWORK_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    message(STATUS "Link-Time Optimization (LTO) enabled for Release and RelWithDebInfo builds")
else()
    message(STATUS "IPO/LTO not supported: ${error}")
    set(ROSCRAFT_BRIDGE_NETWORK_ENABLE_LTO OFF)
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

if(ROSCRAFT_BRIDGE_NETWORK_ENABLE_ROS2)
    find_package(ament_cmake REQUIRED)
    find_package(rclcpp REQUIRED)
endif()

set(ROSCRAFT_BRIDGE_NETWORK_HEADERS
    include/roscraft/bridge/network/bridge_network.hpp
)

set(ROSCRAFT_BRIDGE_NETWORK_SOURCES
)

set(ROSCRAFT_BRIDGE_NETWORK_ALL_FILES
    ${ROSCRAFT_BRIDGE_NETWORK_HEADERS}
    ${ROSCRAFT_BRIDGE_NETWORK_SOURCES}
)

add_library(roscraft_bridge_network_lib STATIC ${ROSCRAFT_BRIDGE_NETWORK_ALL_FILES})
add_library(roscraft::bridge::network ALIAS roscraft_bridge_network_lib)

include(${ROSCRAFT_WS_DIR}/cmake/TargetUtils.cmake)

roscraft_target_set_cxx_standard(roscraft_bridge_network_lib STANDARD 23)
roscraft_target_set_platform(roscraft_bridge_network_lib)
roscraft_target_set_output_dirs(roscraft_bridge_network_lib)
roscraft_target_set_folder(roscraft_bridge_network_lib "roscraft/bridge/network")

roscraft_target_add_pch(roscraft_bridge_network_lib ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.hpp)
roscraft_target_set_warnings(roscraft_bridge_network_lib)
roscraft_target_set_optimization(roscraft_bridge_network_lib)
roscraft_target_enable_lto(roscraft_bridge_network_lib)
roscraft_target_enable_sanitizers(roscraft_bridge_network_lib)

target_compile_definitions(roscraft_bridge_network_lib PUBLIC
    $<$<BOOL:${ROSCRAFT_BRIDGE_NETWORK_ENABLE_ROS2}>:ROSCRAFT_BRIDGE_ENABLE_ROS2>
)

target_include_directories(roscraft_bridge_network_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)



roscraft_require_dependency(argparse)
roscraft_require_dependency(FlatBuffers)
roscraft_require_dependency(glaze)
roscraft_require_dependency(Asio)

target_link_libraries(roscraft_bridge_network_lib
    PUBLIC
        roscraft::bridge::common
    PRIVATE
        roscraft::argparse
        roscraft::flatbuffers
        roscraft::glaze
        roscraft::asio
    )

if(ROSCRAFT_BRIDGE_NETWORK_ENABLE_ROS2)
    ament_target_dependencies(roscraft_bridge_network_lib
        rclcpp
    )
endif()

add_executable(roscraft_bridge_network ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

roscraft_target_set_cxx_standard(roscraft_bridge_network STANDARD 23)
roscraft_target_set_platform(roscraft_bridge_network)
roscraft_target_set_output_dirs(roscraft_bridge_network)
roscraft_target_set_folder(roscraft_bridge_network "roscraft/bridge/network")

roscraft_target_add_pch(roscraft_bridge_network ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.hpp)
roscraft_target_set_warnings(roscraft_bridge_network)
roscraft_target_set_optimization(roscraft_bridge_network)
roscraft_target_enable_lto(roscraft_bridge_network)
roscraft_target_enable_sanitizers(roscraft_bridge_network)

target_include_directories(roscraft_bridge_network PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(roscraft_bridge_network PRIVATE
    roscraft::bridge::network
)

if(ROSCRAFT_BRIDGE_NETWORK_ENABLE_ROS2)
    ament_target_dependencies(roscraft_bridge_network
        rclcpp
    )
endif()

install(TARGETS roscraft_bridge_network_lib roscraft_bridge_network
    EXPORT export_roscraft_bridge_network
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)

# Configure tests
if(BUILD_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ROSCRAFT_BRIDGE_NETWORK_ENABLE_ROS2)
    ament_export_include_directories(include)
    ament_export_targets(export_roscraft_bridge_network HAS_LIBRARY_TARGET)
    ament_export_dependencies(rclcpp)
    ament_package()
endif()

# Print dependency summary after everything is configured
roscraft_print_dependency_summary()
