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

set(
    QGC_MAVLINK_GIT_TAG
    "main"
    CACHE STRING "THACO custom MAVLink revision"
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
