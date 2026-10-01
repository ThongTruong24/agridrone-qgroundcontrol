# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

@AGENTS.md

The imported AGENTS.md covers upstream QGroundControl conventions, `just` recipes, and the Definition of Done.
This file adds what is specific to this fork: the THACO **AgriDrone** custom build living in `custom/`.

## Build, run, test (this machine)

```bash
cmake --build build --config Debug --parallel                                       # incremental build
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe ./build/Debug/QGroundControl        # run (software GL under WSL)

ctest --test-dir build -R CompanionControllerTest --output-on-failure               # one test via CTest
./build/Debug/QGroundControl --unittest:CompanionControllerTest                     # one test via the binary
./build/Debug/QGroundControl --unittest --label=Unit                                # by label
QGC_TEST_VERBOSE=1 ./build/Debug/QGroundControl --unittest:CompanionVehicleLifecycleTest
```

Tests are compiled into the QGroundControl binary (needs `QGC_BUILD_TESTING`). See test/README.md for base classes and `MultiSignalSpy`.

## How the custom build plugs into QGC

`custom/` is picked up automatically by the top-level CMake (QGC's custom-build mechanism). `custom/CMakeLists.txt`:

- Builds the `AgriDroneModule` static lib + QML module `Custom.AgriDrone` (resource prefix `/qml`). **Every new `.qml` file must be added to `AGRIDRONE_QML_FILES`**; QML singletons also need `QT_QML_SINGLETON_TYPE` set (see `CompanionUiAdapter.qml`).
- **Every new C++ source must be added to `CUSTOM_SOURCES`.** Test sources go in `AGRIDRONE_TEST_SOURCES`, and each test class needs both `UT_REGISTER_TEST(...)` in the `.cc` and an `add_qgc_test(Name LABELS ...)` line. (`custom/test/CcTelemetryControllerTest.*` exists but is not currently registered in CMake.)
- Sets `CUSTOMCLASS=CustomPlugin`, so `custom/src/CustomPlugin` replaces `QGCCorePlugin`.

`custom/cmake/CustomOverrides.cmake` runs before the main configure. It:
- Disables the stock PX4 firmware plugin factory. `CustomFirmwarePluginFactory`, `CustomFirmwarePlugin`, and `CustomAutoPilotPlugin` subclass the PX4 ones and add the `AgriDroneComponent` vehicle-setup page.
- Points `QGC_MAVLINK_GIT_REPO`/`QGC_MAVLINK_GIT_TAG` at the THACO MAVLink fork (dialect `all`). Custom messages such as `CC_TELEMETRY_*` come from there. Per AGENTS.md, message definitions are edited only in `/home/lnh/Mavlink/custom/thaco_common.xml` and propagated with `/home/lnh/Mavlink/sync_all.sh`. Private command IDs that can't extend the generated enum live in `custom/src/MAVLink/THACOMAVLink.h`.

### QML override mechanism

`CustomOverrideInterceptor` (in `CustomPlugin.cc`) rewrites any `qrc:/X` URL to `qrc:/Custom/X` when that resource exists. To replace an upstream QML file, add a file at the matching path under the `/Custom` prefix (see `custom/custom.qrc`, `custom/res/Custom/`). Don't edit the upstream file.

The Settings sidebar uses the same trick. `custom/tools/generate_settings_pages.py` merges `src/AppSettings/pages/*.json` with `custom/src/AppSettings/SettingsPages.custom.json` at build time and emits an overriding `SettingsPagesModel.qml`. To add a settings page, edit the custom JSON manifest, not upstream `SettingsPages.json`.

## Companion-computer telemetry architecture

The companion computer (camera, vision, network, links, mission) talks to QGC over MAVLink.

- `CompanionController` is a `QML_SINGLETON`, the single QML-facing façade with a large `Q_PROPERTY` surface. It tracks `MultiVehicleManager::activeVehicleChanged` (guarded `_activeVehicle`, which falls back to `activeVehicle()`), listens to `MAVLinkProtocol::messageReceived`, runs a telemetry watchdog, and drives config apply/confirm flows with timeouts and retries (`_configTimer`, `_confirmTimer`, COMMAND_ACK filtering).
- `CompanionMavlinkDispatcher` fans incoming messages out to `ITelemetryHandler` implementations (`handleMavlinkMessage` / `resetState`). New message families should become a handler registered with the dispatcher rather than more `switch` cases in the controller.
- `CompanionLinksService` (UART/link config, an `ITelemetryHandler`) and `CompanionLogService` (MAVLink/STATUSTEXT log feed) are owned by the controller and re-exposed through its signals.
- `CcTelemetryController` decodes the `CC_TELEMETRY_{LINKS,CAMERA,NETWORK,VISION}` messages for the `qml/CcTelemetry/` page.
- `AgriDroneController` backs the AgriDrone Fly View button/drop panel and settings.
- QML: `CompanionSettings.qml` hosts the tabbed `Companion*Tab.qml` panels. `CompanionUiAdapter.qml` (singleton) and `CcTelemetry/CcTelemetryAdapter.qml` sit between views and C++ state, e.g. holding draft values so comboboxes don't jump while an apply is in flight.

Tests: `custom/test/CompanionControllerTest` holds both `CompanionControllerTest` (Unit; uses `friend` access) and `CompanionVehicleLifecycleTest` (Integration, `VehicleTestManualConnect`, which covers disconnect/reconnect reset, UART ACK retry, and the QML draft/save guard).
