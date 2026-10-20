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
immediately and every second, on the link that carries the Companion heartbeat
(component 191), the same link used for its parameters. This needs no FC or
Vehicle, and never uses the FC's link or the first connected link; until a
Companion heartbeat is seen the flags are only kept in the app. Click or tap
inside the displayed video to send one `MAV_CMD_CAMERA_TRACK_POINT` to the
Companion on that link, with coordinates normalized to the displayed video area.

Bench check (MK15 with a UDP link to the Pi, e.g. `192.168.144.141:14550`): keep
the FC disconnected and another UDP link open, toggle the three flags, and watch
`/cc/ai_vision_control` on the Pi. Repeat with the FC connected; both cases must
use the Companion link.

## Bandwidth

Open the Companion indicator and expand the popup: the Bandwidth table shows, for
telemetry and camera, what the CC sends at each hop (`CC_BANDWIDTH`, 1 Hz) next to
what this QGC receives, as 3 s means. "no data (edge-xxx not reporting)" means that
container sent nothing fresh, which is not the same as 0. "This QGC receives"
normally reads 1–3 % below the CC video out (RTP overhead). How to read the hops:
`Companion_Computer/src/modules/mavlink/README.md`, "Reading the bandwidth table".
