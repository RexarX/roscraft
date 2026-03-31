# Main dependency configuration for Roscraft
#
# This file provides:
# - Initial setup and status messages
# - Test dependencies (loaded here since tests are built)
# - Summary printing at the end of configuration
#
# Strategy: System packages -> CPM fallback

include_guard(GLOBAL)

# Load dependency management helpers
include(DependencyFinder)
include(DownloadUsingCPM)

# Print configuration header
message(STATUS "")
message(STATUS "========== Roscraft Dependency Configuration ==========")
message(STATUS "  → Dependencies loaded on-demand")
message(STATUS "  → System packages checked FIRST")
message(STATUS "  → CPM downloads for missing dependencies")
message(STATUS "Allow CPM downloads: ${ROSCRAFT_${MODULE_NAME}_DOWNLOAD_PACKAGES}")
message(STATUS "Check package versions: ${ROSCRAFT_${MODULE_NAME}_CHECK_PACKAGE_VERSIONS}")
message(STATUS "=====================================================")
message(STATUS "")

if(BUILD_TESTING)
    message(STATUS "Finding Test Dependencies...")
    message(STATUS "")

    roscraft_require_dependency(doctest)

    message(STATUS "")
endif()

function(roscraft_print_dependency_summary)
    roscraft_print_dependencies()
    if(CPM_PACKAGES)
        roscraft_print_cpm_packages()
    endif()
    message(STATUS "")
endfunction()
