#!/usr/bin/env bash
set -euo pipefail

QGC_SOURCE_ROOT="$(git rev-parse --show-toplevel)"
QGC_BUILD_DIR="${BUILD_DIR:-$QGC_SOURCE_ROOT/build}"
QGC_QT_ROOT="${QT_ROOT:-$HOME/Qt/6.11.1/gcc_64}"
BUILD_JOBS="${BUILD_JOBS:-3}"

if [[ ! "$BUILD_JOBS" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERROR: BUILD_JOBS must be a positive integer" >&2
    exit 1
fi
CPU_COUNT="$(nproc)"
if (( BUILD_JOBS > CPU_COUNT )); then
    BUILD_JOBS="$CPU_COUNT"
fi

# Configure every time so CustomOverrides resolves the latest MAVLink main.
cmake -S "$QGC_SOURCE_ROOT" -B "$QGC_BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$QGC_QT_ROOT"

if command -v ionice >/dev/null 2>&1; then
    nice -n 10 ionice -c2 -n7 cmake --build "$QGC_BUILD_DIR" --parallel "$BUILD_JOBS"
else
    nice -n 10 cmake --build "$QGC_BUILD_DIR" --parallel "$BUILD_JOBS"
fi
