# The custom factory inherits the standard PX4 plugin and adds AgriDrone to
# Vehicle Setup. Disable only the stock PX4 factory to avoid duplicate matches.
set(QGC_DISABLE_PX4_PLUGIN_FACTORY ON CACHE BOOL "Disable PX4 Plugin Factory" FORCE)

# ============================================================
# THACO CUSTOM MAVLINK
# ============================================================

# If Drone_MAVLink exists as a sibling directory (in THACO_Drone meta-repo),
# use it directly to ensure zero network latency and 100% local synchronization.
set(THACO_ROOT "${CMAKE_SOURCE_DIR}/.." CACHE PATH "THACO_Drone meta-repository")
if(NOT EXISTS "${THACO_ROOT}/cmake/THACOMAVLink.cmake")
    message(FATAL_ERROR "Build this custom QGC inside THACO_Drone, or set THACO_ROOT to its checkout")
endif()
set(QGC_THACO_MAVLINK ON)

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
