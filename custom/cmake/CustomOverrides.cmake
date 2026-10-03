# The custom factory inherits the standard PX4 plugin and adds AgriDrone to
# Vehicle Setup. Disable only the stock PX4 factory to avoid duplicate matches.
set(QGC_DISABLE_PX4_PLUGIN_FACTORY ON CACHE BOOL "Disable PX4 Plugin Factory" FORCE)

# ============================================================
# THACO CUSTOM MAVLINK
# ============================================================

# If Drone_MAVLink exists as a sibling directory (in THACO_Drone meta-repo),
# use it directly to ensure zero network latency and 100% local synchronization.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Drone_MAVLink/CMakeLists.txt")
    set(CPM_mavlink_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../Drone_MAVLink" CACHE PATH "Path to local MAVLink source" FORCE)
    message(STATUS "QGC: Using local MAVLink submodule at: ${CPM_mavlink_SOURCE}")
else()
    set(
        QGC_MAVLINK_GIT_REPO
        "https://github.com/ThongTruong24/agridrone-mavlink.git"
        CACHE STRING "THACO custom MAVLink repository"
        FORCE
    )

    set(
        QGC_MAVLINK_GIT_TAG
        "181947bb1c76f53076b29a0298f2238a37bb91a4"
        CACHE STRING "THACO custom MAVLink revision"
        FORCE
    )
endif()

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