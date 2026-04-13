roscraft_require_dependency(doctest)

set(ROSCRAFT_BRIDGE_JNI_TEST_SOURCES
    main.cpp

    config.cpp
    env_guard.cpp
    bridge.cpp
    native_methods.cpp

    command/handler.cpp
    command/handler_registry.cpp
    command/callback.cpp
    command/handlers.cpp
)

add_executable(roscraft_bridge_jni_tests ${ROSCRAFT_BRIDGE_JNI_TEST_SOURCES})

roscraft_target_set_cxx_standard(roscraft_bridge_jni_tests STANDARD 23)
roscraft_target_set_platform(roscraft_bridge_jni_tests)
roscraft_target_set_output_dirs(roscraft_bridge_jni_tests CUSTOM_FOLDER tests)
roscraft_target_set_folder(roscraft_bridge_jni_tests "roscraft/tests/bridge/jni")

roscraft_target_set_warnings(roscraft_bridge_jni_tests)
roscraft_target_set_optimization(roscraft_bridge_jni_tests)
roscraft_target_enable_lto(roscraft_bridge_jni_tests)
roscraft_target_enable_sanitizers(roscraft_bridge_jni_tests)

target_include_directories(roscraft_bridge_jni_tests SYSTEM PRIVATE
    ${JNI_INCLUDE_DIRS}
)

target_link_libraries(roscraft_bridge_jni_tests PRIVATE
    roscraft::bridge::jni
    roscraft::doctest::doctest
)

add_test(NAME roscraft_bridge_jni_tests COMMAND roscraft_bridge_jni_tests)

set_tests_properties(roscraft_bridge_jni_tests PROPERTIES
    ENVIRONMENT
        "ASAN_OPTIONS=new_delete_type_mismatch=0"
)
