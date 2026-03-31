# doctest dependency configuration
#
# This module handles finding doctest from multiple sources:
# 1. System packages (pacman, apt, etc.)
# 2. CPM download (fallback)
#
# Usage:
#   roscraft_require_dependency(doctest)

include_guard(GLOBAL)

# Check if already processed
roscraft_dep_is_processed(NAME "doctest" OUTPUT_VAR _doctest_processed)
if(_doctest_processed)
    return()
endif()

roscraft_dep_header(NAME "doctest")

# Use roscraft_module system for standard package finding
roscraft_dep_begin(
    NAME doctest
    VERSION 2.4.11
    DEBIAN_NAMES doctest-dev
    RPM_NAMES doctest-devel
    PACMAN_NAMES doctest
    BREW_NAMES doctest
    PKG_CONFIG_NAMES doctest
    CPM_NAME doctest
    CPM_VERSION 2.4.11
    CPM_GITHUB_REPOSITORY doctest/doctest
    CPM_GIT_TAG v2.4.11
    CPM_OPTIONS
        "DOCTEST_WITH_TESTS OFF"
        "DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF"
)

roscraft_dep_end()

# Create roscraft::doctest::doctest alias if doctest was found
if(NOT TARGET roscraft::doctest::doctest)
    if(TARGET doctest::doctest)
        # Check if it's an alias and get the real target
        get_target_property(_doctest_aliased doctest::doctest ALIASED_TARGET)
        if(_doctest_aliased)
            add_library(roscraft::doctest::doctest ALIAS ${_doctest_aliased})
        else()
            add_library(roscraft::doctest::doctest ALIAS doctest::doctest)
        endif()
        roscraft_dep_log(SUCCESS "doctest configured (doctest::doctest)")
    elseif(TARGET doctest)
        # Check if it's an alias and get the real target
        get_target_property(_doctest_aliased doctest ALIASED_TARGET)
        if(_doctest_aliased)
            add_library(roscraft::doctest::doctest ALIAS ${_doctest_aliased})
        else()
            add_library(roscraft::doctest::doctest ALIAS doctest)
        endif()
        roscraft_dep_log(SUCCESS "doctest configured (doctest)")
    else()
        roscraft_dep_log(NOT_FOUND "doctest")
    endif()
else()
    roscraft_dep_log(SUCCESS "doctest configured (roscraft::doctest::doctest)")
endif()

# Create roscraft::doctest convenience target that brings in all doctest targets
if(NOT TARGET _roscraft_doctest_all)
    add_library(_roscraft_doctest_all INTERFACE)
    if(TARGET roscraft::doctest::doctest)
        target_link_libraries(_roscraft_doctest_all INTERFACE roscraft::doctest::doctest)
    endif()
endif()

if(NOT TARGET roscraft::doctest)
    add_library(roscraft::doctest ALIAS _roscraft_doctest_all)
endif()
