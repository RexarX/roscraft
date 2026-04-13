roscraft_require_dependency(doctest)

set(ROSCRAFT_BRIDGE_COMMON_TEST_SOURCES
    main.cpp

    bridge.cpp

    app/app.cpp
    app/config.cpp

    command/command.cpp
    command/commands.cpp
    command/queue.cpp

    nodes/graph.cpp
    nodes/topic_relay.cpp
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
)

add_test(NAME roscraft_bridge_common_tests COMMAND roscraft_bridge_common_tests)

set_tests_properties(roscraft_bridge_common_tests PROPERTIES
    ENVIRONMENT
        "ASAN_OPTIONS=new_delete_type_mismatch=0;LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp"
)
