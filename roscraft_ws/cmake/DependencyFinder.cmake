# Helper macros for finding and configuring external dependencies
#
# Strategy: System packages -> CPM fallback
#
# Key Features:
# - Cached dependency results to avoid duplicate processing
# - Helper functions for structural logging
# - Automatic target creation and aliasing

include_guard(GLOBAL)

cmake_policy(SET CMP0054 NEW)

# Initialize global variables for dependency tracking.
# _ROSCRAFT_${MODULE_NAME}_DEPENDENCIES_FOUND uses CACHE INTERNAL so "was this package located on
# the system?" survives incremental rebuilds and avoids re-running find_package.
#
# _ROSCRAFT_${MODULE_NAME}_DEPENDENCIES_PROCESSED intentionally uses a GLOBAL PROPERTY (in-memory
# only) so the "already processed this file" flag resets on every CMake invocation.
# This is critical: dependency .cmake files create IMPORTED targets that only live
# for one cmake run. If the processed flag were cached, a regeneration triggered by
# Ninja would find the flag TRUE (from the previous run's cache), skip re-including
# the dependency file, and then fail when target_link_libraries tries to reference
# an IMPORTED target that was never re-created.
if(NOT DEFINED _ROSCRAFT_${MODULE_NAME}_DEPENDENCIES_FOUND)
    set(_ROSCRAFT_${MODULE_NAME}_DEPENDENCIES_FOUND "" CACHE INTERNAL "List of found dependencies")
endif()

# Options for package management
option(ROSCRAFT_${MODULE_NAME}_DOWNLOAD_PACKAGES "Download missing packages using CPM" ON)
option(ROSCRAFT_${MODULE_NAME}_FORCE_DOWNLOAD_PACKAGES "Force download all packages even if system version exists" OFF)
option(ROSCRAFT_${MODULE_NAME}_CHECK_PACKAGE_VERSIONS "Check and enforce package version requirements" ON)

# ============================================================================
# Logging Helper Functions
# ============================================================================

# Function: roscraft_dep_log
#
# Logs a message related to dependency configuration.
# Provides consistent formatting for dependency-related messages.
#
# Usage:
#   roscraft_dep_log(MESSAGE "message text")
#   roscraft_dep_log(STATUS "status message")
#   roscraft_dep_log(WARNING "warning message")
#   roscraft_dep_log(ERROR "error message")
#   roscraft_dep_log(SUCCESS "success message")
#   roscraft_dep_log(DOWNLOAD "downloading message")
#   roscraft_dep_log(CONFIGURE "configuring message")
#
function(roscraft_dep_log)
    set(options "")
    set(oneValueArgs MESSAGE STATUS WARNING ERROR SUCCESS DOWNLOAD CONFIGURE FOUND NOT_FOUND)
    set(multiValueArgs "")

    cmake_parse_arguments(LOG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(LOG_MESSAGE)
        message(STATUS "${LOG_MESSAGE}")
    elseif(LOG_STATUS)
        message(STATUS "  ${LOG_STATUS}")
    elseif(LOG_WARNING)
        message(WARNING "  ⚠ ${LOG_WARNING}")
    elseif(LOG_ERROR)
        message(FATAL_ERROR "  ✗ ${LOG_ERROR}")
    elseif(LOG_SUCCESS)
        message(STATUS "  ✓ ${LOG_SUCCESS}")
    elseif(LOG_DOWNLOAD)
        message(STATUS "  ⬇ ${LOG_DOWNLOAD}")
    elseif(LOG_CONFIGURE)
        message(STATUS "  → ${LOG_CONFIGURE}")
    elseif(LOG_FOUND)
        message(STATUS "  ✓ ${LOG_FOUND} found")
    elseif(LOG_NOT_FOUND)
        message(STATUS "  ✗ ${LOG_NOT_FOUND} not found")
    endif()
endfunction()

# Function: roscraft_dep_header
#
# Logs a header message for a dependency section.
#
# Usage:
#   roscraft_dep_header(NAME "Boost")
#
function(roscraft_dep_header)
    cmake_parse_arguments(HDR "" "NAME" "" ${ARGN})

    if(HDR_NAME)
        message(STATUS "Configuring ${HDR_NAME} dependency...")
    endif()
endfunction()

# ============================================================================
# Caching Functions
# ============================================================================

# Function: roscraft_dep_is_processed
#
# Checks if a dependency file has already been processed.
# Returns TRUE if the dependency was already processed, FALSE otherwise.
#
# Usage:
#   roscraft_dep_is_processed(NAME "spdlog" OUTPUT_VAR was_processed)
#
function(roscraft_dep_is_processed)
    cmake_parse_arguments(ARG "" "NAME;OUTPUT_VAR" "" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "roscraft_dep_is_processed: NAME is required")
    endif()

    if(NOT ARG_OUTPUT_VAR)
        message(FATAL_ERROR "roscraft_dep_is_processed: OUTPUT_VAR is required")
    endif()

    string(TOUPPER "${ARG_NAME}" _upper_name)
    string(REPLACE "-" "_" _upper_name "${_upper_name}")

    # Use a GLOBAL PROPERTY so this flag is per-cmake-run only.
    # IMPORTED targets (roscraft::flat_map, roscraft::stduuid::stduuid, etc.) are
    # in-memory and vanish when cmake exits.  If we used CACHE INTERNAL here,
    # a Ninja-triggered regeneration would see the stale TRUE flag, skip
    # re-including the dependency file, and then fail when target_link_libraries
    # references the now-missing IMPORTED target.
    get_property(_is_processed GLOBAL PROPERTY ROSCRAFT_${MODULE_NAME}_DEP_${_upper_name}_PROCESSED)
    if(_is_processed)
        set(${ARG_OUTPUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${ARG_OUTPUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Function: roscraft_dep_mark_processed
#
# Marks a dependency as processed to avoid duplicate processing.
#
# Usage:
#   roscraft_dep_mark_processed(NAME "spdlog")
#
function(roscraft_dep_mark_processed)
    cmake_parse_arguments(ARG "" "NAME" "" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "roscraft_dep_mark_processed: NAME is required")
    endif()

    string(TOUPPER "${ARG_NAME}" _upper_name)
    string(REPLACE "-" "_" _upper_name "${_upper_name}")

    # Store as a GLOBAL PROPERTY (in-memory, resets each cmake run) so that
    # dependency .cmake files are always re-included on regeneration, allowing
    # IMPORTED targets to be re-created.  See roscraft_dep_is_processed for the
    # full rationale.
    set_property(GLOBAL PROPERTY ROSCRAFT_${MODULE_NAME}_DEP_${_upper_name}_PROCESSED TRUE)

    # Keep a global list for the summary printer (also in-memory only).
    get_property(_processed_list GLOBAL PROPERTY ROSCRAFT_${MODULE_NAME}_DEPENDENCIES_PROCESSED_LIST)
    list(APPEND _processed_list "${ARG_NAME}")
    list(REMOVE_DUPLICATES _processed_list)
    set_property(GLOBAL PROPERTY ROSCRAFT_${MODULE_NAME}_DEPENDENCIES_PROCESSED_LIST "${_processed_list}")
endfunction()

# Function: roscraft_dep_is_found
#
# Checks if a dependency has been found previously.
# Uses CACHE INTERNAL so successful find_package / CPM results survive
# incremental builds without re-searching.
#
# Usage:
#   roscraft_dep_is_found(NAME "spdlog" OUTPUT_VAR was_found)
#
function(roscraft_dep_is_found)
    cmake_parse_arguments(ARG "" "NAME;OUTPUT_VAR" "" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "roscraft_dep_is_found: NAME is required")
    endif()

    if(NOT ARG_OUTPUT_VAR)
        message(FATAL_ERROR "roscraft_dep_is_found: OUTPUT_VAR is required")
    endif()

    string(TOUPPER "${ARG_NAME}" _upper_name)
    string(REPLACE "-" "_" _upper_name "${_upper_name}")

    if(DEFINED ROSCRAFT_${MODULE_NAME}_DEP_${_upper_name}_FOUND AND ROSCRAFT_${MODULE_NAME}_DEP_${_upper_name}_FOUND)
        set(${ARG_OUTPUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${ARG_OUTPUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Function: roscraft_dep_mark_found
#
# Marks a dependency as found.
#
# Usage:
#   roscraft_dep_mark_found(NAME "spdlog" [VIA "system (CONFIG)"])
#
function(roscraft_dep_mark_found)
    cmake_parse_arguments(ARG "" "NAME;VIA" "" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "roscraft_dep_mark_found: NAME is required")
    endif()

    string(TOUPPER "${ARG_NAME}" _upper_name)
    string(REPLACE "-" "_" _upper_name "${_upper_name}")

    # CACHE INTERNAL: persist "was found" across incremental rebuilds so we don't
    # re-run find_package on every regeneration.  This is safe because roscraft_dep_is_found
    # only gates informational/skip logic inside roscraft_dep_begin, not target creation.
    # Target creation always happens in the dependency file body, which is re-run on
    # every cmake invocation thanks to roscraft_dep_is_processed using GLOBAL PROPERTY.
    set(ROSCRAFT_${MODULE_NAME}_DEP_${_upper_name}_FOUND TRUE CACHE INTERNAL "Dependency ${ARG_NAME} was found")

    if(ARG_VIA)
        set(ROSCRAFT_${MODULE_NAME}_DEP_${_upper_name}_FOUND_VIA "${ARG_VIA}" CACHE INTERNAL "How ${ARG_NAME} was found")
    endif()

    list(APPEND _ROSCRAFT_${MODULE_NAME}_DEPENDENCIES_FOUND "${ARG_NAME}")
    list(REMOVE_DUPLICATES _ROSCRAFT_${MODULE_NAME}_DEPENDENCIES_FOUND)
    set(_ROSCRAFT_${MODULE_NAME}_DEPENDENCIES_FOUND "${_ROSCRAFT_${MODULE_NAME}_DEPENDENCIES_FOUND}" CACHE INTERNAL "List of found dependencies")
endfunction()

# ============================================================================
# Dependency Include Helper
# ============================================================================

# Macro: roscraft_require_dependency
#
# Includes a dependency file if the dependency hasn't been processed yet.
#
# Usage:
#   roscraft_require_dependency(spdlog)
#   roscraft_require_dependency(Boost)
#
# The macro will look for cmake/dependencies/{name}.cmake or the file path provided.
#
macro(roscraft_require_dependency _dep_name)
    roscraft_dep_is_processed(NAME "${_dep_name}" OUTPUT_VAR _already_processed)

    if(NOT _already_processed)
        # Try to find the dependency file
        set(_dep_file "${ROSCRAFT_WS_DIR}/cmake/dependencies/${_dep_name}.cmake")

        if(EXISTS "${_dep_file}")
            include("${_dep_file}")
        else()
            # Try lowercase
            string(TOLOWER "${_dep_name}" _dep_name_lower)
            set(_dep_file_lower "${ROSCRAFT_WS_DIR}/cmake/dependencies/${_dep_name_lower}.cmake")

            if(EXISTS "${_dep_file_lower}")
                include("${_dep_file_lower}")
            else()
                message(WARNING "Dependency file not found for '${_dep_name}' (tried: ${_dep_file}, ${_dep_file_lower})")
            endif()
        endif()
    endif()

    unset(_already_processed)
    unset(_dep_file)
    unset(_dep_file_lower)
    unset(_dep_name_lower)
endmacro()

# ============================================================================
# Package Finding Macros
# ============================================================================

# Macro to begin package search
# Usage: roscraft_dep_begin(
#     NAME <name>
#     [VERSION <version>]
#     [DEBIAN_NAMES <pkg1> <pkg2> ...]
#     [RPM_NAMES <pkg1> <pkg2> ...]
#     [PACMAN_NAMES <pkg1> <pkg2> ...]
#     [BREW_NAMES <pkg1> <pkg2> ...]
#     [PKG_CONFIG_NAMES <pkg1> <pkg2> ...]
#     [CPM_NAME <name>]
#     [CPM_VERSION <version>]
#     [CPM_GITHUB_REPOSITORY <repo>]
#     [CPM_URL <url>]
#     [CPM_OPTIONS <opt1> <opt2> ...]
#     [CPM_DOWNLOAD_ONLY]
# )
macro(roscraft_dep_begin)
    set(options CPM_DOWNLOAD_ONLY)
    set(oneValueArgs NAME VERSION CPM_NAME CPM_VERSION CPM_GITHUB_REPOSITORY CPM_URL CPM_GIT_TAG CPM_SOURCE_SUBDIR)
    set(multiValueArgs DEBIAN_NAMES RPM_NAMES PACMAN_NAMES BREW_NAMES PKG_CONFIG_NAMES CPM_OPTIONS)

    cmake_parse_arguments(ROSCRAFT_${MODULE_NAME}_PKG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(_PKG_NAME "${ROSCRAFT_${MODULE_NAME}_PKG_NAME}")

    # Check if already processed - if so, skip everything
    roscraft_dep_is_processed(NAME "${_PKG_NAME}" OUTPUT_VAR _PKG_ALREADY_PROCESSED)
    if(_PKG_ALREADY_PROCESSED)
        # Check if the package was found
        roscraft_dep_is_found(NAME "${_PKG_NAME}" OUTPUT_VAR _pkg_was_found)
        if(_pkg_was_found)
            set("${_PKG_NAME}_FOUND" TRUE)
        endif()
        set("${_PKG_NAME}_SKIP_ROSCRAFT_${MODULE_NAME}_FIND" TRUE)
        unset(_pkg_was_found)
    else()
        # Set up CPM name if not provided
        if(NOT ROSCRAFT_${MODULE_NAME}_PKG_CPM_NAME)
            set(ROSCRAFT_${MODULE_NAME}_PKG_CPM_NAME "${ROSCRAFT_${MODULE_NAME}_PKG_NAME}")
        endif()

        string(TOUPPER "${ROSCRAFT_${MODULE_NAME}_PKG_CPM_NAME}" _PKG_CPM_NAME_UPPER)
        string(REPLACE "-" "_" _PKG_CPM_NAME_UPPER "${_PKG_CPM_NAME_UPPER}")

        # If this package was previously resolved via CPM in this build tree,
        # skip expensive system probing on subsequent re-configures.
        string(TOUPPER "${_PKG_NAME}" _PKG_NAME_UPPER)
        string(REPLACE "-" "_" _PKG_NAME_UPPER "${_PKG_NAME_UPPER}")
        set(_PKG_FORCE_CPM FALSE)
        if(DEFINED ROSCRAFT_${MODULE_NAME}_DEP_${_PKG_NAME_UPPER}_FOUND_VIA AND
           ROSCRAFT_${MODULE_NAME}_DEP_${_PKG_NAME_UPPER}_FOUND_VIA STREQUAL "CPM")
            set(_PKG_FORCE_CPM TRUE)
        endif()

        # Create download options for this package
        option(
            ROSCRAFT_${MODULE_NAME}_DOWNLOAD_${_PKG_CPM_NAME_UPPER}
            "Download and setup ${ROSCRAFT_${MODULE_NAME}_PKG_CPM_NAME} if not found"
            ${ROSCRAFT_${MODULE_NAME}_DOWNLOAD_PACKAGES}
        )
        option(
            ROSCRAFT_${MODULE_NAME}_FORCE_DOWNLOAD_${_PKG_CPM_NAME_UPPER}
            "Force download ${ROSCRAFT_${MODULE_NAME}_PKG_CPM_NAME} even if system package exists"
            ${ROSCRAFT_${MODULE_NAME}_FORCE_DOWNLOAD_PACKAGES}
        )

        # Set version requirement
        if(ROSCRAFT_${MODULE_NAME}_PKG_VERSION)
            if(NOT ${_PKG_NAME}_FIND_VERSION OR "${${_PKG_NAME}_FIND_VERSION}" VERSION_LESS "${ROSCRAFT_${MODULE_NAME}_PKG_VERSION}")
                set("${_PKG_NAME}_FIND_VERSION" "${ROSCRAFT_${MODULE_NAME}_PKG_VERSION}")
            endif()
        endif()

        # Skip version checks if disabled
        if(NOT ROSCRAFT_${MODULE_NAME}_CHECK_PACKAGE_VERSIONS)
            unset("${_PKG_NAME}_FIND_VERSION")
        endif()

        # Check if already found via target
        if(TARGET ${_PKG_NAME} OR TARGET roscraft::${_PKG_NAME})
            if(NOT ${_PKG_NAME}_FIND_VERSION)
                set("${_PKG_NAME}_FOUND" ON)
                set("${_PKG_NAME}_SKIP_ROSCRAFT_${MODULE_NAME}_FIND" ON)
                roscraft_dep_mark_processed(NAME "${_PKG_NAME}")
                roscraft_dep_mark_found(NAME "${_PKG_NAME}" VIA "existing target")
                return()
            endif()

            if(${_PKG_NAME}_VERSION)
                if(${_PKG_NAME}_FIND_VERSION VERSION_LESS_EQUAL ${_PKG_NAME}_VERSION)
                    set("${_PKG_NAME}_FOUND" ON)
                    set("${_PKG_NAME}_SKIP_ROSCRAFT_${MODULE_NAME}_FIND" ON)
                    roscraft_dep_mark_processed(NAME "${_PKG_NAME}")
                    roscraft_dep_mark_found(NAME "${_PKG_NAME}" VIA "existing target")
                    return()
                else()
                    message(FATAL_ERROR
                        "Already using version ${${_PKG_NAME}_VERSION} of ${_PKG_NAME} "
                        "when version ${${_PKG_NAME}_FIND_VERSION} was requested."
                    )
                endif()
            endif()
        endif()

        # Build error message for missing packages
        set(_ERROR_MESSAGE "Could not find `${_PKG_NAME}` package.")
        if(ROSCRAFT_${MODULE_NAME}_PKG_DEBIAN_NAMES)
            list(JOIN ROSCRAFT_${MODULE_NAME}_PKG_DEBIAN_NAMES " " _pkg_names)
            string(APPEND _ERROR_MESSAGE "\n\tDebian/Ubuntu: sudo apt install ${_pkg_names}")
        endif()
        if(ROSCRAFT_${MODULE_NAME}_PKG_RPM_NAMES)
            list(JOIN ROSCRAFT_${MODULE_NAME}_PKG_RPM_NAMES " " _pkg_names)
            string(APPEND _ERROR_MESSAGE "\n\tFedora/RHEL: sudo dnf install ${_pkg_names}")
        endif()
        if(ROSCRAFT_${MODULE_NAME}_PKG_PACMAN_NAMES)
            list(JOIN ROSCRAFT_${MODULE_NAME}_PKG_PACMAN_NAMES " " _pkg_names)
            string(APPEND _ERROR_MESSAGE "\n\tArch Linux: sudo pacman -S ${_pkg_names}")
        endif()
        if(ROSCRAFT_${MODULE_NAME}_PKG_BREW_NAMES)
            list(JOIN ROSCRAFT_${MODULE_NAME}_PKG_BREW_NAMES " " _pkg_names)
            string(APPEND _ERROR_MESSAGE "\n\tmacOS: brew install ${_pkg_names}")
        endif()
        string(APPEND _ERROR_MESSAGE "\n")

        # Initialize search result variables
        set("${_PKG_NAME}_LIBRARIES")
        set("${_PKG_NAME}_INCLUDE_DIRS")
        set("${_PKG_NAME}_EXECUTABLE")
        set("${_PKG_NAME}_FOUND" FALSE)
    endif()
endmacro()

# Helper to search for package components
macro(_roscraft_dep_find_part)
    set(options)
    set(oneValueArgs PART_TYPE)
    set(multiValueArgs NAMES PATHS PATH_SUFFIXES)

    cmake_parse_arguments(PART "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(${_PKG_NAME}_SKIP_ROSCRAFT_${MODULE_NAME}_FIND)
        return()
    endif()

    # Determine variable name based on type
    if("${PART_PART_TYPE}" STREQUAL "library")
        set(_variable "${_PKG_NAME}_LIBRARIES")
        find_library(${_PKG_NAME}_LIBRARIES_${PART_NAMES}
            NAMES ${PART_NAMES}
            PATHS ${PART_PATHS}
            PATH_SUFFIXES ${PART_PATH_SUFFIXES}
        )
        list(APPEND "${_variable}" "${${_PKG_NAME}_LIBRARIES_${PART_NAMES}}")
    elseif("${PART_PART_TYPE}" STREQUAL "include")
        set(_variable "${_PKG_NAME}_INCLUDE_DIRS")
        find_path(${_PKG_NAME}_INCLUDE_DIRS_${PART_NAMES}
            NAMES ${PART_NAMES}
            PATHS ${PART_PATHS}
            PATH_SUFFIXES ${PART_PATH_SUFFIXES}
        )
        list(APPEND "${_variable}" "${${_PKG_NAME}_INCLUDE_DIRS_${PART_NAMES}}")
    elseif("${PART_PART_TYPE}" STREQUAL "program")
        set(_variable "${_PKG_NAME}_EXECUTABLE")
        find_program(${_PKG_NAME}_EXECUTABLE_${PART_NAMES}
            NAMES ${PART_NAMES}
            PATHS ${PART_PATHS}
            PATH_SUFFIXES ${PART_PATH_SUFFIXES}
        )
        list(APPEND "${_variable}" "${${_PKG_NAME}_EXECUTABLE_${PART_NAMES}}")
    else()
        message(FATAL_ERROR "Invalid PART_TYPE: ${PART_PART_TYPE}")
    endif()
endmacro()

# Find library component
macro(roscraft_dep_find_library)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs NAMES PATHS PATH_SUFFIXES)

    cmake_parse_arguments(LIB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    _roscraft_dep_find_part(
        PART_TYPE library
        NAMES ${LIB_NAMES}
        PATHS ${LIB_PATHS}
        PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
    )
endmacro()

# Find include component
macro(roscraft_dep_find_include)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs NAMES PATHS PATH_SUFFIXES)

    cmake_parse_arguments(INC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    _roscraft_dep_find_part(
        PART_TYPE include
        NAMES ${INC_NAMES}
        PATHS ${INC_PATHS}
        PATH_SUFFIXES ${INC_PATH_SUFFIXES}
    )
endmacro()

# Find program component
macro(roscraft_dep_find_program)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs NAMES PATHS PATH_SUFFIXES)

    cmake_parse_arguments(PROG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    _roscraft_dep_find_part(
        PART_TYPE program
        NAMES ${PROG_NAMES}
        PATHS ${PROG_PATHS}
        PATH_SUFFIXES ${PROG_PATH_SUFFIXES}
    )
endmacro()

# Finalize package search
macro(roscraft_dep_end)
    if(${_PKG_NAME}_SKIP_ROSCRAFT_${MODULE_NAME}_FIND)
        return()
    endif()

    # Try to find via standard mechanisms
    set(_FOUND_VIA "")

    # 1. Try system packages first (unless force download is enabled)
    if(NOT ${_PKG_NAME}_FOUND AND NOT ROSCRAFT_${MODULE_NAME}_FORCE_DOWNLOAD_${_PKG_CPM_NAME_UPPER} AND NOT _PKG_FORCE_CPM)
        # Try CONFIG mode first (for CMake-aware packages)
        find_package(${_PKG_NAME} ${${_PKG_NAME}_FIND_VERSION} CONFIG QUIET)
        if(${_PKG_NAME}_FOUND)
            set(_FOUND_VIA "system (CONFIG)")
        else()
            # Try MODULE mode (for Find*.cmake files)
            find_package(${_PKG_NAME} ${${_PKG_NAME}_FIND_VERSION} MODULE QUIET)
            if(${_PKG_NAME}_FOUND)
                set(_FOUND_VIA "system (MODULE)")
            endif()
        endif()
    endif()

    # 2. Try pkg-config
    if(NOT ${_PKG_NAME}_FOUND AND ROSCRAFT_${MODULE_NAME}_PKG_PKG_CONFIG_NAMES AND NOT ROSCRAFT_${MODULE_NAME}_FORCE_DOWNLOAD_${_PKG_CPM_NAME_UPPER} AND NOT _PKG_FORCE_CPM)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            list(GET ROSCRAFT_${MODULE_NAME}_PKG_PKG_CONFIG_NAMES 0 _pkg_config_name)
            pkg_check_modules(${_PKG_NAME}_PC QUIET IMPORTED_TARGET ${_pkg_config_name})
            if(${_PKG_NAME}_PC_FOUND)
                set(${_PKG_NAME}_FOUND TRUE)
                set(_FOUND_VIA "pkg-config")
                if(NOT TARGET ${_PKG_NAME})
                    add_library(${_PKG_NAME} INTERFACE IMPORTED)
                    target_link_libraries(${_PKG_NAME} INTERFACE PkgConfig::${_PKG_NAME}_PC)
                endif()
            endif()
        endif()
    endif()

    # 3. Try manual search if we have search criteria
    set(_required_vars)
    if(NOT "${${_PKG_NAME}_LIBRARIES}" STREQUAL "")
        list(APPEND _required_vars "${_PKG_NAME}_LIBRARIES")
    endif()
    if(NOT "${${_PKG_NAME}_INCLUDE_DIRS}" STREQUAL "")
        list(APPEND _required_vars "${_PKG_NAME}_INCLUDE_DIRS")
    endif()
    if(NOT "${${_PKG_NAME}_EXECUTABLE}" STREQUAL "")
        list(APPEND _required_vars "${_PKG_NAME}_EXECUTABLE")
    endif()

    if(_required_vars AND NOT ${_PKG_NAME}_FOUND AND NOT _PKG_FORCE_CPM)
        include(FindPackageHandleStandardArgs)
        find_package_handle_standard_args(
            ${_PKG_NAME}
            REQUIRED_VARS ${_required_vars}
            VERSION_VAR ${_PKG_NAME}_VERSION
            FAIL_MESSAGE "${_ERROR_MESSAGE}"
        )
        if(${_PKG_NAME}_FOUND)
            set(_FOUND_VIA "manual search")
        endif()
    endif()

    # 4. Try CPM as last resort
    if(NOT ${_PKG_NAME}_FOUND AND ROSCRAFT_${MODULE_NAME}_DOWNLOAD_${_PKG_CPM_NAME_UPPER})
        if(ROSCRAFT_${MODULE_NAME}_PKG_CPM_GITHUB_REPOSITORY OR ROSCRAFT_${MODULE_NAME}_PKG_CPM_URL)
            include(DownloadUsingCPM)
            _roscraft_cpm_add_package()
            if(${_PKG_NAME}_ADDED OR TARGET ${_PKG_NAME})
                set(${_PKG_NAME}_FOUND TRUE)
                set(_FOUND_VIA "CPM")
            endif()
        endif()
    endif()

    # Report results and mark as processed
    roscraft_dep_mark_processed(NAME "${_PKG_NAME}")

    if(${_PKG_NAME}_FOUND)
        roscraft_dep_mark_found(NAME "${_PKG_NAME}" VIA "${_FOUND_VIA}")
        roscraft_dep_log(SUCCESS "${_PKG_NAME} found via ${_FOUND_VIA}")

        # Create interface target if needed
        if(_required_vars AND NOT TARGET ${_PKG_NAME})
            add_library(${_PKG_NAME} INTERFACE IMPORTED GLOBAL)

            if(${_PKG_NAME}_INCLUDE_DIRS)
                set_target_properties(${_PKG_NAME} PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${${_PKG_NAME}_INCLUDE_DIRS}"
                    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${${_PKG_NAME}_INCLUDE_DIRS}"
                )
            endif()

            if(${_PKG_NAME}_LIBRARIES)
                set_target_properties(${_PKG_NAME} PROPERTIES
                    INTERFACE_LINK_LIBRARIES "${${_PKG_NAME}_LIBRARIES}"
                )
            endif()
        endif()

        # Create roscraft:: alias if it doesn't exist
        if(NOT TARGET roscraft::${_PKG_NAME})
            # Try different common target naming patterns
            set(_target_to_alias "")

            if(TARGET ${_PKG_NAME})
                get_target_property(_aliased ${_PKG_NAME} ALIASED_TARGET)
                if(_aliased)
                    set(_target_to_alias ${_aliased})
                else()
                    set(_target_to_alias ${_PKG_NAME})
                endif()
            elseif(TARGET ${_PKG_NAME}::${_PKG_NAME})
                get_target_property(_aliased ${_PKG_NAME}::${_PKG_NAME} ALIASED_TARGET)
                if(_aliased)
                    set(_target_to_alias ${_aliased})
                else()
                    set(_target_to_alias ${_PKG_NAME}::${_PKG_NAME})
                endif()
            elseif(TARGET ${_PKG_NAME}::${_PKG_NAME}_header_only)
                get_target_property(_aliased ${_PKG_NAME}::${_PKG_NAME}_header_only ALIASED_TARGET)
                if(_aliased)
                    set(_target_to_alias ${_aliased})
                else()
                    set(_target_to_alias ${_PKG_NAME}::${_PKG_NAME}_header_only)
                endif()
            endif()

            if(_target_to_alias)
                add_library(roscraft::${_PKG_NAME} ALIAS ${_target_to_alias})

                # Mark target includes as SYSTEM to suppress warnings
                get_target_property(_target_includes ${_target_to_alias} INTERFACE_INCLUDE_DIRECTORIES)
                if(_target_includes)
                    set_target_properties(${_target_to_alias} PROPERTIES
                        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_target_includes}"
                    )
                endif()
            endif()

            unset(_target_to_alias)
            unset(_aliased)
        endif()
    else()
        if(${_PKG_NAME}_FIND_REQUIRED)
            message(FATAL_ERROR "${_ERROR_MESSAGE}")
        else()
            roscraft_dep_log(NOT_FOUND "${_PKG_NAME}")
        endif()
    endif()

    # Clean up
    unset(_FOUND_VIA)
    unset(_required_vars)
    unset(_ERROR_MESSAGE)
    unset(_PKG_NAME)
    unset(_PKG_CPM_NAME_UPPER)
    unset(_PKG_NAME_UPPER)
    unset(_PKG_FORCE_CPM)
    unset(_PKG_ALREADY_PROCESSED)
endmacro()

# Internal helper to add package via CPM
macro(_roscraft_cpm_add_package)
    set(_cpm_args NAME ${ROSCRAFT_${MODULE_NAME}_PKG_CPM_NAME})

    if(ROSCRAFT_${MODULE_NAME}_PKG_CPM_VERSION)
        list(APPEND _cpm_args VERSION ${ROSCRAFT_${MODULE_NAME}_PKG_CPM_VERSION})
    endif()

    if(ROSCRAFT_${MODULE_NAME}_PKG_CPM_GITHUB_REPOSITORY)
        list(APPEND _cpm_args GITHUB_REPOSITORY ${ROSCRAFT_${MODULE_NAME}_PKG_CPM_GITHUB_REPOSITORY})
    endif()

    if(ROSCRAFT_${MODULE_NAME}_PKG_CPM_URL)
        list(APPEND _cpm_args URL ${ROSCRAFT_${MODULE_NAME}_PKG_CPM_URL})
    endif()

    if(ROSCRAFT_${MODULE_NAME}_PKG_CPM_GIT_TAG)
        list(APPEND _cpm_args GIT_TAG ${ROSCRAFT_${MODULE_NAME}_PKG_CPM_GIT_TAG})
    endif()

    if(ROSCRAFT_${MODULE_NAME}_PKG_CPM_SOURCE_SUBDIR)
        list(APPEND _cpm_args SOURCE_SUBDIR ${ROSCRAFT_${MODULE_NAME}_PKG_CPM_SOURCE_SUBDIR})
    endif()

    if(ROSCRAFT_${MODULE_NAME}_PKG_CPM_OPTIONS)
        list(APPEND _cpm_args OPTIONS ${ROSCRAFT_${MODULE_NAME}_PKG_CPM_OPTIONS})
    endif()

    if(ROSCRAFT_${MODULE_NAME}_PKG_CPM_DOWNLOAD_ONLY)
        list(APPEND _cpm_args DOWNLOAD_ONLY YES)
    endif()

    CPMAddPackage(${_cpm_args})

    unset(_cpm_args)
endmacro()

# Function to print all found dependencies
function(roscraft_print_dependencies)
    message(STATUS "========== Roscraft Engine Dependencies ==========")
    if(_ROSCRAFT_${MODULE_NAME}_DEPENDENCIES_FOUND)
        set(_deps_list ${_ROSCRAFT_${MODULE_NAME}_DEPENDENCIES_FOUND})
        list(REMOVE_DUPLICATES _deps_list)
        list(SORT _deps_list)
        foreach(_dep ${_deps_list})
            string(TOUPPER "${_dep}" _upper_dep)
            string(REPLACE "-" "_" _upper_dep "${_upper_dep}")
            if(DEFINED ROSCRAFT_${MODULE_NAME}_DEP_${_upper_dep}_FOUND_VIA)
                message(STATUS "  ✓ ${_dep} (${ROSCRAFT_${MODULE_NAME}_DEP_${_upper_dep}_FOUND_VIA})")
            else()
                message(STATUS "  ✓ ${_dep}")
            endif()
        endforeach()
    else()
        message(STATUS "  No dependencies found")
    endif()
    message(STATUS "================================================")
endfunction()
