roscraft_require_dependency(doctest)

set(ROSCRAFT_CORE_TEST_SOURCES
    assert.cpp
    compiler.cpp
    main.cpp
    platform.cpp
    stacktrace.cpp

    container/inline_callable_buffer.cpp
    container/multi_type_map.cpp
    container/sparse_set.cpp
    container/static_string.cpp
    container/typed_buffer.cpp
    container/typed_buffer_array.cpp

    memory/aligned_alloc.cpp
    memory/allocator_traits.cpp
    memory/arena_allocator.cpp
    memory/common.cpp
    memory/frame_allocator.cpp
    memory/free_list_allocator.cpp
    memory/pool_allocator.cpp
    memory/ref_counted.cpp
    memory/stack_allocator.cpp

    utils/common_traits.cpp
    utils/defer.cpp
    utils/delegate.cpp
    utils/fast_pimpl.cpp
    utils/filesystem.cpp
    utils/hash.cpp
    utils/macro.cpp
    utils/random.cpp
    utils/string_hash.cpp
    utils/timer.cpp
    utils/type_info.cpp
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
