roscraft_require_dependency(doctest)

set(ROSCRAFT_CORE_TEST_SOURCES
    main.cpp
)

add_executable(roscraft_core_tests ${ROSCRAFT_CORE_TEST_SOURCES})

roscraft_target_set_cxx_standard(roscraft_core_tests STANDARD 23)
roscraft_target_set_platform(roscraft_core_tests)
roscraft_target_set_output_dirs(roscraft_core_tests CUSTOM_FOLDER tests)
roscraft_target_set_folder(roscraft_core_tests "roscraft/tests/core")

roscraft_target_set_warnings(roscraft_core_tests)
roscraft_target_set_optimization(roscraft_core_tests)
roscraft_target_enable_lto(roscraft_core_tests)
roscraft_target_enable_sanitizers(roscraft_core_tests)

target_link_libraries(roscraft_core_tests PRIVATE
    roscraft::core
    roscraft::doctest::doctest
)

add_test(NAME roscraft_core_tests COMMAND roscraft_core_tests)
