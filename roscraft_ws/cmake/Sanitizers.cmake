# ============================================================================
# Sanitizer Configuration for Roscraft
# ============================================================================
#
# This module provides support for various C++ sanitizers:
# - AddressSanitizer (ASan): Detects memory errors (buffer overflows, use-after-free, etc.)
# - UndefinedBehaviorSanitizer (UBSan): Detects undefined behavior
# - ThreadSanitizer (TSan): Detects data races (mutually exclusive with ASan)
# - MemorySanitizer (MSan): Detects uninitialized memory reads (Clang only, requires instrumented libc++)
#
# Usage:
#   include(Sanitizers)
#   roscraft_target_enable_sanitizers(my_target)
#
# Options:
#   DEVELOPER_MODE                 - Expose sanitizer options and functionality (default: ON for top-level)
#   ROSCRAFT_ENABLE_SANITIZERS       - Master switch for sanitizers (default: ON for Debug)
#   ROSCRAFT_SANITIZER_ADDRESS       - Enable AddressSanitizer (default: ON)
#   ROSCRAFT_SANITIZER_UNDEFINED     - Enable UndefinedBehaviorSanitizer (default: ON)
#   ROSCRAFT_SANITIZER_THREAD        - Enable ThreadSanitizer (default: OFF, mutually exclusive with ASan)
#   ROSCRAFT_SANITIZER_MEMORY        - Enable MemorySanitizer (default: OFF, Clang only)
#
# Notes:
#   - ASan and TSan cannot be used together
#   - MSan requires the entire program (including libc++) to be built with MSan
#   - MSVC only supports ASan (/fsanitize=address)
#   - Sanitizers are typically only enabled for Debug builds
#
# ============================================================================

include_guard(GLOBAL)

# Sanitizers are a developer-only feature.
if(NOT DEVELOPER_MODE)
    function(roscraft_target_enable_sanitizers TARGET)
    endfunction()

    function(roscraft_print_sanitizer_status)
        message(STATUS "Sanitizers: DISABLED (DEVELOPER_MODE=OFF)")
    endfunction()

    return()
endif()

# Detect compiler capabilities
set(ROSCRAFT_COMPILER_IS_GNU OFF)
set(ROSCRAFT_COMPILER_IS_CLANG OFF)
set(ROSCRAFT_COMPILER_IS_MSVC OFF)
set(ROSCRAFT_COMPILER_IS_CLANG_CL OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(ROSCRAFT_COMPILER_IS_GNU ON)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(ROSCRAFT_COMPILER_IS_CLANG_CL ON)
    else()
        set(ROSCRAFT_COMPILER_IS_CLANG ON)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(ROSCRAFT_COMPILER_IS_MSVC ON)
endif()

# ============================================================================
# Sanitizer Options
# ============================================================================

# Master switch - enabled by default for Debug builds
option(ROSCRAFT_ENABLE_SANITIZERS "Enable sanitizers for Debug builds" ON)

# Individual sanitizer options
option(ROSCRAFT_SANITIZER_ADDRESS "Enable AddressSanitizer" ON)
option(ROSCRAFT_SANITIZER_UNDEFINED "Enable UndefinedBehaviorSanitizer" ON)
option(ROSCRAFT_SANITIZER_THREAD "Enable ThreadSanitizer (mutually exclusive with ASan)" OFF)
option(ROSCRAFT_SANITIZER_MEMORY "Enable MemorySanitizer (Clang only, requires instrumented libc++)" OFF)

# ============================================================================
# Validation
# ============================================================================

# Check for mutually exclusive sanitizers
if(ROSCRAFT_SANITIZER_ADDRESS AND ROSCRAFT_SANITIZER_THREAD)
    message(WARNING "AddressSanitizer and ThreadSanitizer cannot be used together. Disabling ThreadSanitizer.")
    set(ROSCRAFT_SANITIZER_THREAD OFF CACHE BOOL "Enable ThreadSanitizer" FORCE)
endif()

if(ROSCRAFT_SANITIZER_ADDRESS AND ROSCRAFT_SANITIZER_MEMORY)
    message(WARNING "AddressSanitizer and MemorySanitizer cannot be used together. Disabling MemorySanitizer.")
    set(ROSCRAFT_SANITIZER_MEMORY OFF CACHE BOOL "Enable MemorySanitizer" FORCE)
endif()

if(ROSCRAFT_SANITIZER_THREAD AND ROSCRAFT_SANITIZER_MEMORY)
    message(WARNING "ThreadSanitizer and MemorySanitizer cannot be used together. Disabling MemorySanitizer.")
    set(ROSCRAFT_SANITIZER_MEMORY OFF CACHE BOOL "Enable MemorySanitizer" FORCE)
endif()

# MSan is only available on Clang
if(ROSCRAFT_SANITIZER_MEMORY AND NOT ROSCRAFT_COMPILER_IS_CLANG)
    message(WARNING "MemorySanitizer is only available with Clang. Disabling MemorySanitizer.")
    set(ROSCRAFT_SANITIZER_MEMORY OFF CACHE BOOL "Enable MemorySanitizer" FORCE)
endif()

# MSVC only supports ASan
if(ROSCRAFT_COMPILER_IS_MSVC OR ROSCRAFT_COMPILER_IS_CLANG_CL)
    if(ROSCRAFT_SANITIZER_UNDEFINED)
        message(STATUS "UndefinedBehaviorSanitizer is not supported on MSVC. Disabling.")
        set(ROSCRAFT_SANITIZER_UNDEFINED OFF CACHE BOOL "Enable UndefinedBehaviorSanitizer" FORCE)
    endif()
    if(ROSCRAFT_SANITIZER_THREAD)
        message(STATUS "ThreadSanitizer is not supported on MSVC. Disabling.")
        set(ROSCRAFT_SANITIZER_THREAD OFF CACHE BOOL "Enable ThreadSanitizer" FORCE)
    endif()
    if(ROSCRAFT_SANITIZER_MEMORY)
        message(STATUS "MemorySanitizer is not supported on MSVC. Disabling.")
        set(ROSCRAFT_SANITIZER_MEMORY OFF CACHE BOOL "Enable MemorySanitizer" FORCE)
    endif()
endif()

# ============================================================================
# Internal Helper Functions
# ============================================================================

# Build the sanitizer flags string for GCC/Clang
function(_roscraft_get_sanitizer_flags OUT_COMPILE_FLAGS OUT_LINK_FLAGS)
    set(_compile_flags "")
    set(_link_flags "")

    if(ROSCRAFT_COMPILER_IS_GNU OR ROSCRAFT_COMPILER_IS_CLANG)
        set(_sanitizers "")

        if(ROSCRAFT_SANITIZER_ADDRESS)
            list(APPEND _sanitizers "address")
        endif()

        if(ROSCRAFT_SANITIZER_UNDEFINED)
            list(APPEND _sanitizers "undefined")
        endif()

        if(ROSCRAFT_SANITIZER_THREAD)
            list(APPEND _sanitizers "thread")
        endif()

        if(ROSCRAFT_SANITIZER_MEMORY)
            list(APPEND _sanitizers "memory")
        endif()

        if(_sanitizers)
            list(JOIN _sanitizers "," _sanitizer_list)
            set(_compile_flags "-fsanitize=${_sanitizer_list} -fno-omit-frame-pointer -fno-optimize-sibling-calls")
            set(_link_flags "-fsanitize=${_sanitizer_list}")

            # Add extra flags for better error reporting
            if(ROSCRAFT_SANITIZER_ADDRESS)
                string(APPEND _compile_flags " -fsanitize-address-use-after-scope")
            endif()

            if(ROSCRAFT_SANITIZER_UNDEFINED)
                # Print stack trace on UBSan error
                string(APPEND _compile_flags " -fno-sanitize-recover=undefined")
            endif()
        endif()
    endif()

    set(${OUT_COMPILE_FLAGS} "${_compile_flags}" PARENT_SCOPE)
    set(${OUT_LINK_FLAGS} "${_link_flags}" PARENT_SCOPE)
endfunction()

# Build the sanitizer flags for MSVC
function(_roscraft_get_msvc_sanitizer_flags OUT_COMPILE_FLAGS OUT_LINK_FLAGS)
    set(_compile_flags "")
    set(_link_flags "")

    if(ROSCRAFT_COMPILER_IS_MSVC OR ROSCRAFT_COMPILER_IS_CLANG_CL)
        if(ROSCRAFT_SANITIZER_ADDRESS)
            set(_compile_flags "/fsanitize=address")
            # MSVC ASan doesn't require special linker flags
        endif()
    endif()

    set(${OUT_COMPILE_FLAGS} "${_compile_flags}" PARENT_SCOPE)
    set(${OUT_LINK_FLAGS} "${_link_flags}" PARENT_SCOPE)
endfunction()

# ============================================================================
# Public API
# ============================================================================

# Enable sanitizers for a specific target
# Usage: roscraft_target_enable_sanitizers(my_target)
function(roscraft_target_enable_sanitizers TARGET)
    if(NOT ROSCRAFT_ENABLE_SANITIZERS)
        return()
    endif()

    # Only apply sanitizers for Debug builds
    set(_is_debug "$<CONFIG:Debug>")

    if(ROSCRAFT_COMPILER_IS_MSVC OR ROSCRAFT_COMPILER_IS_CLANG_CL)
        _roscraft_get_msvc_sanitizer_flags(_compile_flags _link_flags)

        if(_compile_flags)
            target_compile_options(${TARGET} PRIVATE
                $<${_is_debug}:${_compile_flags}>
            )
        endif()

        if(_link_flags)
            target_link_options(${TARGET} PRIVATE
                $<${_is_debug}:${_link_flags}>
            )
        endif()
    else()
        _roscraft_get_sanitizer_flags(_compile_flags _link_flags)

        if(_compile_flags)
            separate_arguments(_compile_flags_list UNIX_COMMAND "${_compile_flags}")
            target_compile_options(${TARGET} PRIVATE
                $<${_is_debug}:${_compile_flags_list}>
            )
        endif()

        if(_link_flags)
            separate_arguments(_link_flags_list UNIX_COMMAND "${_link_flags}")
            target_link_options(${TARGET} PRIVATE
                $<${_is_debug}:${_link_flags_list}>
            )
        endif()
    endif()
endfunction()

# Print sanitizer configuration status
function(roscraft_print_sanitizer_status)
    if(NOT ROSCRAFT_ENABLE_SANITIZERS)
        message(STATUS "Sanitizers: DISABLED")
        return()
    endif()

    message(STATUS "")
    message(STATUS "========== Sanitizer Configuration ==========")
    message(STATUS "Sanitizers enabled for Debug builds")

    if(ROSCRAFT_COMPILER_IS_MSVC OR ROSCRAFT_COMPILER_IS_CLANG_CL)
        message(STATUS "  Compiler: MSVC/clang-cl (limited sanitizer support)")
        if(ROSCRAFT_SANITIZER_ADDRESS)
            message(STATUS "  ✓ AddressSanitizer")
        endif()
    else()
        message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
        if(ROSCRAFT_SANITIZER_ADDRESS)
            message(STATUS "  ✓ AddressSanitizer")
        endif()
        if(ROSCRAFT_SANITIZER_UNDEFINED)
            message(STATUS "  ✓ UndefinedBehaviorSanitizer")
        endif()
        if(ROSCRAFT_SANITIZER_THREAD)
            message(STATUS "  ✓ ThreadSanitizer")
        endif()
        if(ROSCRAFT_SANITIZER_MEMORY)
            message(STATUS "  ✓ MemorySanitizer")
        endif()
    endif()

    message(STATUS "==============================================")
    message(STATUS "")
endfunction()
