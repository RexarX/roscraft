roscraft_require_dependency(doctest)

find_package(std_srvs REQUIRED)
find_package(rclcpp REQUIRED)

set(ROSCRAFT_BRIDGE_COMMON_TEST_SOURCES
    main.cpp

    bridge.cpp

    app/app.cpp
    app/config.cpp

    command/command.cpp
    command/commands.cpp
    command/queue.cpp
    command/handler_registry.cpp

    command/handlers/addon.cpp
    command/handlers/action.cpp
    command/handlers/error.cpp
    command/handlers/graph.cpp
    command/handlers/interface.cpp
    command/handlers/node.cpp
    command/handlers/param.cpp
    command/handlers/player.cpp
    command/handlers/service.cpp
    command/handlers/topic.cpp

    nodes/ros/interface/list.cpp
    nodes/ros/interface/show.cpp
    nodes/ros/node/info.cpp
    nodes/ros/action/info.cpp
    nodes/ros/action/send_goal.cpp

    nodes/addon/event.cpp

    nodes/ros/service/info.cpp
    nodes/ros/service/call.cpp
    nodes/ros/topic/info.cpp
    nodes/ros/topic/relay.cpp
    nodes/ros/topic/stats.cpp
    nodes/ros/graph/cache.cpp
    nodes/ros/param/get.cpp
    nodes/ros/param/set.cpp
    nodes/ros/param/list.cpp
    nodes/ros/param/describe.cpp
    nodes/ros/param/dump.cpp
    nodes/ros/param/load.cpp
)

add_executable(roscraft_bridge_common_tests ${ROSCRAFT_BRIDGE_COMMON_TEST_SOURCES})

roscraft_target_set_cxx_standard(roscraft_bridge_common_tests STANDARD 23)
roscraft_target_set_platform(roscraft_bridge_common_tests)
roscraft_target_set_output_dirs(roscraft_bridge_common_tests CUSTOM_FOLDER tests)
roscraft_target_set_folder(roscraft_bridge_common_tests "roscraft/tests/bridge/common")

roscraft_target_set_warnings(roscraft_bridge_common_tests)
roscraft_target_set_optimization(roscraft_bridge_common_tests)
roscraft_target_enable_lto(roscraft_bridge_common_tests)
roscraft_target_enable_sanitizers(roscraft_bridge_common_tests)

target_link_libraries(roscraft_bridge_common_tests PRIVATE
    roscraft::bridge::common
    roscraft::doctest::doctest
    std_srvs::std_srvs__rosidl_typesupport_cpp
    rclcpp::rclcpp
)

add_test(NAME roscraft_bridge_common_tests COMMAND roscraft_bridge_common_tests --no-intro)

set_tests_properties(roscraft_bridge_common_tests PROPERTIES
    ENVIRONMENT
        "ASAN_OPTIONS=new_delete_type_mismatch=0;LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp"
)
