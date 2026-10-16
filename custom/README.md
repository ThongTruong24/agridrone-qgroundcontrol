# AgriDrone QGroundControl

## Build

```bash
./custom/tools/build-qgc.sh
```

The script configures an incremental Debug build, limits parallel jobs (default: 3),
and lowers CPU and I/O priority. Override `BUILD_JOBS`, `BUILD_DIR`, or `QT_ROOT`
as needed.

MAVLink is generated from `Drone_MAVLink/message_definitions/v1.0/thaco.xml`
in the THACO_Drone meta-repository through `cmake/THACOMAVLink.cmake` and
`sync_mavlink.py`. For a standalone QGC checkout, configure with
`-DTHACO_ROOT=/path/to/THACO_Drone`. Never edit generated headers.

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
