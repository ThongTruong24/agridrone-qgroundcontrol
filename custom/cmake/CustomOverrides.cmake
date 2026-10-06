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

# Pin the last revision which provides the telemetry schema consumed by this
# custom tree. The next upstream revision replaces CC_TELEMETRY_LINKS with a
# different wire format and requires a coordinated telemetry migration.
set(_mavlink_revision_file "${CMAKE_SOURCE_DIR}/${QGC_CUSTOM_DIR}/mavlink-revision.txt")
file(STRINGS "${_mavlink_revision_file}" _mavlink_revision LIMIT_COUNT 1)
string(LENGTH "${_mavlink_revision}" _mavlink_revision_length)
if(NOT _mavlink_revision MATCHES "^[0-9a-f]+$" OR NOT _mavlink_revision_length EQUAL 40)
    message(FATAL_ERROR "Invalid MAVLink revision in ${_mavlink_revision_file}: ${_mavlink_revision}")
endif()

set(QGC_MAVLINK_GIT_TAG "${_mavlink_revision}" CACHE STRING "THACO custom MAVLink revision" FORCE)
message(STATUS "THACO MAVLink revision: ${QGC_MAVLINK_GIT_TAG}")

# CPM's patch step must also accept revisions which already contain a QGC patch.
set(
    PATCH_EXECUTABLE
    "${CMAKE_SOURCE_DIR}/${QGC_CUSTOM_DIR}/tools/apply-patch-idempotent.sh"
    CACHE FILEPATH "Idempotent patch wrapper for custom dependencies"
    FORCE
)

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
