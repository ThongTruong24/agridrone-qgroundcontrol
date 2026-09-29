# CC Telemetry Qt Quick Preview

Standalone Qt 6.11.1 application for developing the four CC Telemetry tabs without building QGroundControl or connecting MAVLink hardware. The presentation files are shared directly with the QGC custom build from `custom/qml/CcTelemetry`; only the preview host and mock source live here.

The preview is intentionally isolated from QGC's root CMake project and Android targets. Its raw mock object uses the exact field names from the four messages in the THACO dialect. `CcTelemetryAdapter.qml` is the seam where the mock can later be replaced by decoded MAVLink data.

See [MAVLINK_MAPPING.md](MAVLINK_MAPPING.md) for the audited field-to-UI mapping and the two explicitly derived states.

## Windows quick start

The helper locates the Visual Studio copy of CMake, Qt's Ninja, and initializes the MSVC environment. This avoids relying on `cmake` already being in `PATH`.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools/qml-preview/build-preview.ps1 -Run
```

Optional arguments:

```powershell
tools/qml-preview/build-preview.cmd `
  -QtPath C:/QtS/6.11.1/msvc2022_64 `
  -BuildDirectory build-qml-preview `
  -Run
```

The executable is `build-qml-preview/cc-qml-preview.exe` by default. Pass `--no-watch` to disable automatic reload or `--quit-after 1500` for a smoke test.

## Live reload

Saving any `.qml` file under `tools/qml-preview/qml` or `custom/qml/CcTelemetry` reloads the whole QML tree after a 180 ms debounce. `Ctrl+R` triggers the same reload manually. The C++ executable does not need to be rebuilt for QML-only edits because it loads both source directories while developing.

## Qt Creator

1. Open `tools/qml-preview/CMakeLists.txt` as a standalone project.
2. Select the **Desktop Qt 6.11.1 MSVC 2022 64-bit** kit.
3. Configure and build target `cc-qml-preview`.
4. Run that target and edit files under `tools/qml-preview/qml`.

Do not open the repository's root `CMakeLists.txt` for this preview workflow.

## VS Code

Install CMake Tools and a Qt/QML extension, then add this local workspace setting if the repository root is open:

```json
{
  "cmake.sourceDirectory": "${workspaceFolder}/tools/qml-preview",
  "cmake.buildDirectory": "${workspaceFolder}/build-qml-preview"
}
```

Select the Qt 6.11.1 MSVC kit, run **CMake: Configure**, **CMake: Build**, then start `build-qml-preview/cc-qml-preview.exe`. The helper script above is also safe to run in VS Code's PowerShell terminal when `cmake` is not globally available.

## Source layout

- `qml/MockCcTelemetry.qml`: raw mock packets, using exact MAVLink field names.
- `qml/Main.qml`: preview window and live-reload host.
- `qml/MockCcTelemetry.qml`: preview-only raw mock packets using exact MAVLink field names.
- `custom/qml/CcTelemetry/CcTelemetryAdapter.qml`: shared data adapter and formatting.
- `custom/qml/CcTelemetry/CcTelemetryPanel.qml`: shared title, tab navigation, and responsive content host.
- `custom/qml/CcTelemetry/*Tab.qml`: shared Links, Camera, Network, and Vision presentation.
- `custom/qml/CcTelemetry/{SectionCard,MetricCard,...}.qml`: shared visual components.

The preview therefore expects to be built from this QGroundControl checkout; it is independent of the main QGC build target, but intentionally shares its QML presentation source.
