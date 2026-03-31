cmake_minimum_required(VERSION 3.22)
project(roscraft_interfaces)

find_package(ament_cmake REQUIRED)

set(_interface_files)
file(GLOB _msg_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} msg/*.msg)
file(GLOB _srv_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} srv/*.srv)
file(GLOB _action_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} action/*.action)

list(APPEND _interface_files ${_msg_files})
list(APPEND _interface_files ${_srv_files})
list(APPEND _interface_files ${_action_files})

if(_interface_files)
    find_package(rosidl_default_generators REQUIRED)

    rosidl_generate_interfaces(${PROJECT_NAME}
        ${_interface_files}
    )

    ament_export_dependencies(rosidl_default_runtime)
endif()

ament_package()
