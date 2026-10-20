# CLAUDE.md

Always use Context7 when I need library/API documentation, code generation, setup or configuration steps without me having to explicitly ask.

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

@AGENTS.md

The imported AGENTS.md covers upstream QGroundControl conventions, `just` recipes, and the Definition of Done.
This file adds what is specific to this fork: the THACO **AgriDrone** custom build living in `custom/`.

## Build, run, test (this machine)

```bash
cmake --build build --config Debug --parallel                                       # incremental build
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe ./build/Debug/AgriDroneGCS        # run (software GL under WSL)
# Do not judge video quality in the WSL build: WSL2's monotonic clock runs ~7% slow (2026-10-14), so the clock-synced
# sink falls behind and rtspsrc drop-on-latency drops packets -> macroblocking/lag. Use native Windows QGC, VLC or the SIYI handheld.

ctest --test-dir build -R CompanionControllerTest --output-on-failure               # one test via CTest
./build/Debug/AgriDroneGCS --unittest:CompanionControllerTest                     # one test via the binary
./build/Debug/AgriDroneGCS --unittest --label=Unit                                # by label
QGC_TEST_VERBOSE=1 ./build/Debug/AgriDroneGCS --unittest:CompanionVehicleLifecycleTest
```

The app is renamed `AgriDroneGCS` / `com.agridrone.gcs` (ADR-0008); settings saved under the old `QGroundControl` name do not carry over.

Tests are compiled into the AgriDroneGCS binary (needs `QGC_BUILD_TESTING`). See test/README.md for base classes and `MultiSignalSpy`.

## How the custom build plugs into QGC

`custom/` is picked up automatically by the top-level CMake (QGC's custom-build mechanism). `custom/CMakeLists.txt`:

- Builds the `AgriDroneModule` static lib + QML module `Custom.AgriDrone` (resource prefix `/qml`). **Every new `.qml` file must be added to `AGRIDRONE_QML_FILES`**; QML singletons also need `QT_QML_SINGLETON_TYPE` set (`set_source_files_properties` in `custom/CMakeLists.txt`).
- **Every new C++ source must be added to `CUSTOM_SOURCES`.** Test sources go in `AGRIDRONE_TEST_SOURCES`, and each test class needs both `UT_REGISTER_TEST(...)` in the `.cc` and an `add_qgc_test(Name LABELS ...)` line.
- Sets `CUSTOMCLASS=CustomPlugin`, so `custom/src/CustomPlugin` replaces `QGCCorePlugin`.

`custom/cmake/CustomOverrides.cmake` runs before the main configure. It:
- Disables the stock PX4 firmware plugin factory. `CustomFirmwarePluginFactory`, `CustomFirmwarePlugin`, and `CustomAutoPilotPlugin` subclass the PX4 ones and add the `AgriDroneComponent` vehicle-setup page.
- Builds the THACO MAVLink dialect from the meta-repo (`QGC_THACO_MAVLINK`, `cmake/THACOMAVLink.cmake`, `sync_mavlink.py`). Custom messages such as `CC_TELEMETRY_*` come from there. Per AGENTS.md, message definitions are edited only in `Drone_MAVLink/message_definitions/v1.0/thaco.xml`; the build regenerates `MAVLinkEnums.h` when that file changes. Private command IDs that can't extend the generated enum live in `custom/src/MAVLink/THACOMAVLink.h`.

### QML override mechanism

`CustomOverrideInterceptor` (in `CustomPlugin.cc`) rewrites any `qrc:/X` URL to `qrc:/Custom/X` when that resource exists. To replace an upstream QML file, add a file at the matching path under the `/Custom` prefix (see `custom/custom.qrc`, `custom/res/Custom/`). Don't edit the upstream file.

The Settings sidebar uses the same trick. `custom/tools/generate_settings_pages.py` merges `src/AppSettings/pages/*.json` with `custom/src/AppSettings/SettingsPages.custom.json` at build time and emits an overriding `SettingsPagesModel.qml`. To add a settings page, edit the custom JSON manifest, not upstream `SettingsPages.json`.

## Companion-computer telemetry architecture

The companion computer (camera, vision, network, links, mission) talks to QGC over MAVLink.

- `CompanionController` is a `QML_SINGLETON`, the single QML-facing façade with a large `Q_PROPERTY` surface. It works without a `Vehicle` (ADR-0010). It listens to `MAVLinkProtocol::messageReceived` only, runs a telemetry watchdog, and drives the popups' operations (`ToolbarOperation`). **CC link:** the CC heartbeat (compid 191) is enough; the controller keeps the link it arrived on and `CompanionParamService` sends through a `CompanionTransport` built from it (`PARAM_EXT_*`, SET). The controller also sends APPLY and SAVE itself as one `COMMAND_LONG`, **once, never retried**, and matches the `COMMAND_ACK` from compid 191 (IN_PROGRESS + `progress`, ACCEPTED, FAILED); there is no `Vehicle::sendMavCommandWithHandler`. **CC session epoch** (replaces the Vehicle epoch) advances when the CC heartbeat returns after more than 3.5 s or its sysid changes. **Lock rule:** with an active Vehicle only the CC with the Vehicle's sysid is used; with none the first compid-191 sender locks; the lock resets only when a Vehicle of a different id appears. **Lock state:** `configLocked()` is true on bit 32 `CC_SYS_STATUS_CONFIG_LOCKED` of `CC_TELEMETRY_SYSTEM`, on an armed Vehicle, or when the CC just refused; with no Vehicle QGC never assumes "not armed". The reason shown is the CC's own STATUSTEXT (`<Group>: <PARAM>: <reason>`, matched to the pending op), else "CC từ chối (không có lý do)". **Outcomes:** an APPLY (links/hotspot) pending when the heartbeat is lost does not fail: it waits with no deadline ("đang chờ kết nối lại", cancel with `cancelCompanionWait()`), then re-reads the group's parameters. Links are confirmed by that readback, the hotspot only by AP telemetry within a 20 s window; any other op ends with "Mất kết nối Companion". **Unsaved / save-blocked:** a confirmed APPLY makes `unsaved` true (Lưu sends `saveCompanionGroup("links"|"hotspot")`, SAVE param1 1/3); an unconfirmed, cancelled or unanswered APPLY (and a refused hotspot APPLY) sets save-blocked so SAVE never persists an unproven candidate, while a plainly refused links APPLY does not block; a retry whose values equal the CC stage while something is pending sends an APPLY-only op. Each operation keeps a step list and a 0..1 progress that the popups show (`operationState(origin)`); group-prefixed STATUSTEXT (`Links: `, `Hotspot: `, `Camera: `, `Vision: `) become log lines. QGC holds no fact about the CC's links: no FC/SIYI fields, defaults or link-count limit; names come from `CC_SERIAL_LINK`, ports from the read-only `CC_SER_PORTS` (boot scan, `availablePorts()`), values from PARAM_EXT (empty until the CC reports them). Baud options and labels still come from `custom/res/cc_parameters.json` (no `default` keys; a catalog that fails to load blocks every operation) until milestone B (CC-published metadata over `COMPONENT_METADATA` + read-only MAVLink FTP).
- `CompanionMavlinkDispatcher` fans incoming messages out to `ITelemetryHandler` implementations (`handleMavlinkMessage` / `resetState`). New message families should become a handler registered with the dispatcher rather than more `switch` cases in the controller.
- `CompanionLinksService` (UART link telemetry, an `ITelemetryHandler`) and `CompanionParamService` (the CC parameter list, SET and ACK) are owned by the controller and re-exposed through its signals.
- The AI Vision controls, normalized tracking-point commands, mission-trigger telemetry, audit log, and receive-side `CompanionConfigService` from master remain available. MAVLink input uses MAVLinkProtocol once and retains the independent CC session.
- `AgriDroneController` backs the AgriDrone Fly View button/drop panel and settings.
- QML: all Companion controls live in two Fly View toolbar indicators, both `CompanionStatusIndicator.qml`. The camera one (`CameraStatusIndicator.qml`, section `camera`) shows the stream status on top (Connected / Disconnected / Off / No data; in Vision mode Connected also needs fresh Vision telemetry) and the three-position `StreamModeSwitch.qml` (`Video | Off | Vision`) below it. The switch sends `CompanionController.setStreamMode()` (CC_STREAM_MODE 1/0/2), moves only when the CC confirms (`streamModeState.mode`), is dimmed while `pendingMode != mode`, is disabled while `streamModeState.blocked` names why a SET would be refused (a pending Vision CC_VIS_CONF/FPS op or a `streamReason`), replaces a pending mode op or drops a pending camera settings op (the CC ends it too: the way out of a stuck rebuild), restarts the video player whenever the confirmed mode changes (any source), and asks "Tắt camera khi đang bay?" before sending Off while armed. Tapping the icon opens `StreamStatusPage.qml`, one merged popup: left the active stream (from CC_TELEMETRY_CAMERA, plus model/inference fps/confidence in Vision mode), right the switch, `CompanionOperationLog.qml`, a Cancel button (`cancelStreamOperation()`, mode operations only) and the "Cấu hình Video" / "Cấu hình Vision" groups (`setStreamParameter`). The Companion indicator opens `CompanionStatusPage.qml` (UART device/baud, hotspot). There is no Companion page in Settings. Camera, Vision and stream changes are autosaved by the CC; links and hotspot are not: after a confirmed APPLY the popup shows "Đã áp dụng, chưa lưu — reboot sẽ về cấu hình cũ" with Lưu (SAVE) and, while waiting for the CC, Hủy chờ. Camera/Vision operations have no deadline: the CC sends IN_PROGRESS keepalives and `Camera: [k/n] …` lines (progress); only 3 s of silence ends the wait as unconfirmed ("CC không phản hồi"). Camera/Vision stay writable while armed; links/hotspot follow `configLocked()`. The popups need only the CC heartbeat. When a popup cannot be used it shows the reason from `streamReason()` (no heartbeat, no telemetry, old firmware, busy, catalog not loaded). A configured value that is not read yet never locks the camera popup (2026-10-19): `streamState().reading` only shows "Đang đọc tham số…", combos fall back to the running stream's value, and camera/vision SETs go out without a known current value; links/hotspot still wait for every link value.

Tests: `custom/test/CompanionControllerTest` holds both `CompanionControllerTest` (Unit; uses `friend` access) and `CompanionVehicleLifecycleTest` (Integration, `VehicleTestManualConnect`: disconnect/reconnect reset, stream mode progress/cancel/silence, APPLY stages, router/AP restart confirmed by telemetry, reason table, secrets never shown). CC messages (heartbeat, `PARAM_EXT_VALUE`, `COMMAND_ACK`) are injected with `processMessageForTest(msg, link)` and sends are captured with `setSendHookForTest()`; the Vehicle tests pass `mockLink()` so QGC sends on the mock link. The six classes are run with `--unittest:<Class>` (`CompanionControllerTest`, `CompanionVehicleLifecycleTest`, `CompanionParamServiceTest`, `CompanionLinksServiceTest`, `CompanionProtocolTest`, `CompanionMavlinkDispatcherTest`).
