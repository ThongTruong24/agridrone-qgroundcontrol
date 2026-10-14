# CLAUDE.md

Always use Context7 when I need library/API documentation, code generation, setup or configuration steps without me having to explicitly ask.

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

@AGENTS.md

The imported AGENTS.md covers upstream QGroundControl conventions, `just` recipes, and the Definition of Done.
This file adds what is specific to this fork: the THACO **AgriDrone** custom build living in `custom/`.

## Build, run, test (this machine)

```bash
cmake --build build --config Debug --parallel                                       # incremental build
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe ./build/Debug/QGroundControl        # run (software GL under WSL)
# Do not judge video quality in the WSL build: WSL2's monotonic clock runs ~7% slow (2026-10-14), so the clock-synced
# sink falls behind and rtspsrc drop-on-latency drops packets -> macroblocking/lag. Use native Windows QGC, VLC or the SIYI handheld.

ctest --test-dir build -R CompanionControllerTest --output-on-failure               # one test via CTest
./build/Debug/QGroundControl --unittest:CompanionControllerTest                     # one test via the binary
./build/Debug/QGroundControl --unittest --label=Unit                                # by label
QGC_TEST_VERBOSE=1 ./build/Debug/QGroundControl --unittest:CompanionVehicleLifecycleTest
```

Tests are compiled into the QGroundControl binary (needs `QGC_BUILD_TESTING`). See test/README.md for base classes and `MultiSignalSpy`.

## How the custom build plugs into QGC

`custom/` is picked up automatically by the top-level CMake (QGC's custom-build mechanism). `custom/CMakeLists.txt`:

- Builds the `AgriDroneModule` static lib + QML module `Custom.AgriDrone` (resource prefix `/qml`). **Every new `.qml` file must be added to `AGRIDRONE_QML_FILES`**; QML singletons also need `QT_QML_SINGLETON_TYPE` set (`set_source_files_properties` in `custom/CMakeLists.txt`).
- **Every new C++ source must be added to `CUSTOM_SOURCES`.** Test sources go in `AGRIDRONE_TEST_SOURCES`, and each test class needs both `UT_REGISTER_TEST(...)` in the `.cc` and an `add_qgc_test(Name LABELS ...)` line.
- Sets `CUSTOMCLASS=CustomPlugin`, so `custom/src/CustomPlugin` replaces `QGCCorePlugin`.

`custom/cmake/CustomOverrides.cmake` runs before the main configure. It:
- Disables the stock PX4 firmware plugin factory. `CustomFirmwarePluginFactory`, `CustomFirmwarePlugin`, and `CustomAutoPilotPlugin` subclass the PX4 ones and add the `AgriDroneComponent` vehicle-setup page.
- Points `QGC_MAVLINK_GIT_REPO`/`QGC_MAVLINK_GIT_TAG` at the THACO MAVLink fork (dialect `all`). Custom messages such as `CC_TELEMETRY_*` come from there. Per AGENTS.md, message definitions are edited only in `/home/lnh/Mavlink/custom/thaco_common.xml` and propagated with `/home/lnh/Mavlink/sync_all.sh`. Private command IDs that can't extend the generated enum live in `custom/src/MAVLink/THACOMAVLink.h`.

### QML override mechanism

`CustomOverrideInterceptor` (in `CustomPlugin.cc`) rewrites any `qrc:/X` URL to `qrc:/Custom/X` when that resource exists. To replace an upstream QML file, add a file at the matching path under the `/Custom` prefix (see `custom/custom.qrc`, `custom/res/Custom/`). Don't edit the upstream file.

The Settings sidebar uses the same trick. `custom/tools/generate_settings_pages.py` merges `src/AppSettings/pages/*.json` with `custom/src/AppSettings/SettingsPages.custom.json` at build time and emits an overriding `SettingsPagesModel.qml`. To add a settings page, edit the custom JSON manifest, not upstream `SettingsPages.json`.

## Companion-computer telemetry architecture

The companion computer (camera, vision, network, links, mission) talks to QGC over MAVLink.

- `CompanionController` is a `QML_SINGLETON`, the single QML-facing façade with a large `Q_PROPERTY` surface. It tracks `MultiVehicleManager::activeVehicleChanged` (guarded `_activeVehicle`, which falls back to `activeVehicle()`), listens to `MAVLinkProtocol::messageReceived`, runs a telemetry watchdog, and drives the popups' operations (`ToolbarOperation`): PARAM_EXT_SET steps with per-SET ACK timers, then `APPLY_CONFIG` through `Vehicle::sendMavCommandWithHandler` (progress and result handlers), confirmation by telemetry, rollback of staged writes. Each operation keeps a step list and a 0..1 progress that the popups show (`operationState(origin)`); the CC reports real APPLY stages as `COMMAND_ACK IN_PROGRESS` with `progress`, and group-prefixed STATUSTEXT (`Links: `, `Hotspot: `, `Camera: `, `Vision: `) become log lines.
- `CompanionMavlinkDispatcher` fans incoming messages out to `ITelemetryHandler` implementations (`handleMavlinkMessage` / `resetState`). New message families should become a handler registered with the dispatcher rather than more `switch` cases in the controller.
- `CompanionLinksService` (UART link telemetry, an `ITelemetryHandler`) and `CompanionParamService` (the CC parameter list, SET and ACK) are owned by the controller and re-exposed through its signals.
- `AgriDroneController` backs the AgriDrone Fly View button/drop panel and settings.
- QML: all Companion controls live in two Fly View toolbar indicators, both `CompanionStatusIndicator.qml`. The camera one (`CameraStatusIndicator.qml`, section `camera`) shows the stream status on top (Connected / Disconnected / Off / No data; in Vision mode Connected also needs fresh Vision telemetry) and the three-position `StreamModeSwitch.qml` (`Video | Off | Vision`) below it. The switch sends `CompanionController.setStreamMode()` (CC_STREAM_MODE 1/0/2), moves only when the CC confirms (`streamModeState.mode`), is dimmed while `pendingMode != mode`, is disabled while `streamModeState.blocked` names why a SET would be refused (a pending Vision CC_VIS_CONF/FPS op or a `streamReason`), replaces a pending mode op or drops a pending camera settings op (the CC ends it too: the way out of a stuck rebuild), restarts the video player whenever the confirmed mode changes (any source), and asks "Tắt camera khi đang bay?" before sending Off while armed. Tapping the icon opens `StreamStatusPage.qml`, one merged popup: left the active stream (from CC_TELEMETRY_CAMERA, plus model/inference fps/confidence in Vision mode), right the switch, `CompanionOperationLog.qml`, a Cancel button (`cancelStreamOperation()`, mode operations only) and the "Cấu hình Video" / "Cấu hình Vision" groups (`setStreamParameter`). The Companion indicator opens `CompanionStatusPage.qml` (UART device/baud, hotspot). There is no Companion page in Settings: the CC saves every successful change itself (autosave), so there are no Save/Restore buttons. Camera/Vision operations have no deadline: the CC sends IN_PROGRESS keepalives and `Camera: [k/n] …` lines (progress); only 3 s of silence ends the wait as unconfirmed ("CC không phản hồi"). Camera/Vision stay writable while armed; links/hotspot do not. When a popup cannot be used it shows the reason from `streamReason()` (no vehicle, no heartbeat, no telemetry, old firmware, loading, busy).

Tests: `custom/test/CompanionControllerTest` holds both `CompanionControllerTest` (Unit; uses `friend` access) and `CompanionVehicleLifecycleTest` (Integration, `VehicleTestManualConnect`: disconnect/reconnect reset, stream mode progress/cancel/silence, APPLY stages, router/AP restart confirmed by telemetry, reason table, secrets never shown). The CC's APPLY ACKs must be injected with `mockLink()->respondWithMavlinkMessage(...)` because they go through the vehicle's command queue.
