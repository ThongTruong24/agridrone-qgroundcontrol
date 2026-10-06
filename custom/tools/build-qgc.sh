#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# AgriDrone QGroundControl Build Script
#
# - Check latest agridrone-mavlink origin/main
# - Reconfigure QGC when MAVLink commit changes
# - Limit parallel build jobs to avoid exhausting RAM
# - Lower build CPU/I/O priority to keep Ubuntu responsive
#
# Default:
#   BUILD_JOBS=3
#
# Override if needed:
#   BUILD_JOBS=4 ./custom/tools/build-qgc.sh
# ============================================================

ROOT="$(git rev-parse --show-toplevel)"
BUILD="$ROOT/build"

MAVLINK_REPO="https://github.com/ThongTruong24/agridrone-mavlink.git"

# ------------------------------------------------------------
# Build resource limits
# ------------------------------------------------------------

# 3 jobs is recommended for this laptop (~8 GB RAM).
BUILD_JOBS="${BUILD_JOBS:-7}"

CPU_COUNT="$(nproc)"

# Never allow BUILD_JOBS to exceed available CPU threads.
if (( BUILD_JOBS > CPU_COUNT )); then
    BUILD_JOBS="$CPU_COUNT"
fi

echo
echo "============================================================"
echo " AgriDrone QGroundControl Build"
echo "============================================================"
echo "Source       : $ROOT"
echo "Build        : $BUILD"
echo "CPU threads  : $CPU_COUNT"
echo "Build jobs   : $BUILD_JOBS"
echo "============================================================"
echo

# ------------------------------------------------------------
# Check AgriDrone MAVLink
# ------------------------------------------------------------

echo "== Checking AgriDrone MAVLink =="

REMOTE_SHA="$(
    git ls-remote "$MAVLINK_REPO" refs/heads/main |
    awk '{print $1}'
)"

if [[ -z "$REMOTE_SHA" ]]; then
    echo "ERROR: Cannot resolve MAVLink origin/main"
    exit 1
fi

CACHED_SHA=""

if [[ -f "$BUILD/CMakeCache.txt" ]]; then
    CACHED_SHA="$(
        sed -n 's/^QGC_MAVLINK_GIT_TAG:STRING=//p' \
            "$BUILD/CMakeCache.txt" |
        head -n1
    )"
fi

echo "Remote MAVLink : $REMOTE_SHA"
echo "Build MAVLink  : ${CACHED_SHA:-not configured}"
echo

# ------------------------------------------------------------
# Configure QGroundControl
# ------------------------------------------------------------

if [[ "$REMOTE_SHA" != "$CACHED_SHA" ]]; then

    echo "== MAVLink changed or build not configured =="
    echo "== Configuring QGroundControl =="

    cmake \
        -S "$ROOT" \
        -B "$BUILD" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/gcc_64"

else

    echo "== MAVLink already up to date =="

fi

echo

# ------------------------------------------------------------
# Build QGroundControl
# ------------------------------------------------------------

echo "== Building QGroundControl =="
echo "Parallel jobs : $BUILD_JOBS"
echo "CPU priority  : nice +10"
echo "I/O priority  : idle/low"
echo

# Lower CPU priority with nice.
#
# ionice is used when available so the build does not aggressively
# compete with VS Code / GNOME / other interactive applications.

if command -v ionice >/dev/null 2>&1; then

    nice -n 10 \
        ionice -c2 -n7 \
        cmake --build "$BUILD" --parallel "$BUILD_JOBS"

else

    echo "WARNING: ionice not found. Using nice only."

    nice -n 10 \
        cmake --build "$BUILD" --parallel "$BUILD_JOBS"

fi

echo
echo "============================================================"
echo " Build completed successfully"
echo "============================================================"