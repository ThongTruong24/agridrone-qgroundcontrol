# The custom factory inherits the standard PX4 plugin and adds AgriDrone to
# Vehicle Setup. Disable only the stock PX4 factory to avoid duplicate matches.
set(QGC_DISABLE_PX4_PLUGIN_FACTORY ON CACHE BOOL "Disable PX4 Plugin Factory" FORCE)

# ============================================================
# THACO CUSTOM MAVLINK
# ============================================================

set(
    QGC_MAVLINK_GIT_REPO
    "https://github.com/ThongTruong24/agridrone-mavlink.git"
    CACHE STRING "THACO custom MAVLink repository"
    FORCE
)

# Resolve the moving branch before CPM chooses its revision-specific source cache.
find_package(Git REQUIRED)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" ls-remote --exit-code
            "${QGC_MAVLINK_GIT_REPO}" refs/heads/main
    RESULT_VARIABLE _mavlink_result
    OUTPUT_VARIABLE _mavlink_head
    ERROR_VARIABLE _mavlink_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    TIMEOUT 60
)

if(NOT "${_mavlink_result}" STREQUAL "0")
    message(FATAL_ERROR "Cannot resolve MAVLink main (${_mavlink_result}): ${_mavlink_error}")
endif()
if(NOT _mavlink_head MATCHES "^([0-9a-f]+)[ \t]+refs/heads/main$")
    message(FATAL_ERROR "Unexpected MAVLink main response: ${_mavlink_head}")
endif()
set(_mavlink_revision "${CMAKE_MATCH_1}")
string(LENGTH "${_mavlink_revision}" _mavlink_revision_length)
if(NOT _mavlink_revision_length EQUAL 40)
    message(FATAL_ERROR "Invalid MAVLink main commit: ${_mavlink_revision}")
endif()

set(QGC_MAVLINK_GIT_TAG "${_mavlink_revision}" CACHE STRING "THACO custom MAVLink revision" FORCE)
message(STATUS "THACO MAVLink main: ${QGC_MAVLINK_GIT_TAG}")

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
