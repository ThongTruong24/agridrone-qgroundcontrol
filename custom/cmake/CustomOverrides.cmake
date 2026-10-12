# The custom factory inherits the standard PX4 plugin and adds AgriDrone to
# Vehicle Setup. Disable only the stock PX4 factory to avoid duplicate matches.
set(QGC_DISABLE_PX4_PLUGIN_FACTORY ON CACHE BOOL "Disable PX4 Plugin Factory" FORCE)

# ============================================================
# THACO CUSTOM MAVLINK
# ============================================================

# Resolve main at every configure. A commit SHA gives CPM a new cache key when
# the branch advances, without resetting an existing dependency checkout.
set(QGC_THACO_MAVLINK OFF)
set(QGC_MAVLINK_GIT_REPO "https://github.com/ThongTruong24/agridrone-mavlink.git"
    CACHE STRING "THACO custom MAVLink repository" FORCE)
find_package(Git REQUIRED)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" ls-remote --exit-code "${QGC_MAVLINK_GIT_REPO}" refs/heads/main
    RESULT_VARIABLE _mavlink_remote_result
    OUTPUT_VARIABLE _mavlink_remote_output
    ERROR_VARIABLE _mavlink_remote_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    TIMEOUT 30
)
if(NOT _mavlink_remote_result STREQUAL "0")
    message(FATAL_ERROR "Cannot resolve MAVLink main: ${_mavlink_remote_error} (${_mavlink_remote_result})")
endif()
string(REGEX REPLACE "[ \t].*" "" _mavlink_revision "${_mavlink_remote_output}")
string(LENGTH "${_mavlink_revision}" _mavlink_revision_length)
if(NOT _mavlink_revision MATCHES "^[0-9a-f]+$" OR NOT _mavlink_revision_length EQUAL 40)
    message(FATAL_ERROR "Invalid MAVLink main revision: ${_mavlink_remote_output}")
endif()
set(QGC_MAVLINK_GIT_TAG "${_mavlink_revision}" CACHE STRING "Latest THACO MAVLink main revision" FORCE)
message(STATUS "THACO MAVLink main: ${QGC_MAVLINK_GIT_TAG}")

# Current MAVLink revisions may already contain the deterministic-mavgen patch.
set(PATCH_EXECUTABLE "${CMAKE_SOURCE_DIR}/${QGC_CUSTOM_DIR}/tools/apply-patch-idempotent.sh"
    CACHE FILEPATH "Idempotent patch wrapper for custom dependencies" FORCE)

set(
    QGC_MAVLINK_DIALECT
    "all"
    CACHE STRING "MAVLink dialect"
    FORCE
)

set(
    QGC_MAVLINK_VERSION
    "2.0"
    CACHE STRING "MAVLink protocol version"
    FORCE
)
