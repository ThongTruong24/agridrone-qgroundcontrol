# AgriDrone QGroundControl

## Build

```bash
./custom/tools/build-qgc.sh
```

The script configures an incremental Debug build, limits parallel jobs (default: 3),
and lowers CPU and I/O priority. Override `BUILD_JOBS`, `BUILD_DIR`, or `QT_ROOT`
as needed.

Every configure resolves the latest `main` commit from
https://github.com/ThongTruong24/agridrone-mavlink and downloads that exact revision
through CPM. Run the script or `just configure` to refresh MAVLink; an incremental
build alone does not query GitHub. Internet access is required during configure.
The build works directly in this QGC checkout, without a parent-repository CMake
helper. `custom/mavlink-revision.txt` records the original AI branch's older schema
and does not pin the current build.

## Run QGC

```bash
./build/Debug/QGroundControl
```

## AI Vision

The Fly View AI Vision panel controls Bounding Box, Tracking, and Following.
Tracking and Following require Bounding Box. Changes send `CC_AI_VISION_CONTROL`
immediately and every second while a vehicle is connected. Click or tap inside
the displayed video to send one `MAV_CMD_CAMERA_TRACK_POINT` to Companion
component 191, with coordinates normalized to the displayed video area.
