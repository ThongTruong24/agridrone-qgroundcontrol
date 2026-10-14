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

## Android Release APK (SIYI MK15)

The MK15 runs Android 9 (API 28). This fork uses the following toolchain, matching
`.github/build-config.json` and the Qt 6.11 Android prerequisites:

| Component | Version |
| --- | --- |
| QGC baseline | 5.1.4, with the custom Git commit in the APK version |
| Qt host and Android kits | 6.11.1 |
| JDK | 21 |
| Android SDK / Build Tools | 36 / 36.0.0 |
| Android NDK | r27c / 27.2.12479018 |
| Minimum Android | 9 / API 28 |
| GStreamer Android SDK | 1.28.4 |
| APK ABIs | arm64-v8a and armeabi-v7a |

Official references:
[QGC build guide](https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/getting_started/),
[Qt 6.11 prerequisites](https://doc.qt.io/qt-6.11/android-configure-dev-environment.html),
[Qt Android CMake/APK workflow](https://doc.qt.io/qt-6.11/android-building-projects-from-commandline.html),
[MK15 specifications](https://www.siyi.biz/en/product/hand-gcs/mk15-industry/spec/).

On this Linux machine, Qt is installed under `~/Qt/6.11.1`, the SDK under
`~/Android/Sdk`, and JDK/GStreamer under `~/.local/share/thaco-android-build/`.
The local `v5.1.4` tag marks the original imported baseline commit, so CMake
derives a semantic version rather than falling back to `0.0.0`.

```bash
./custom/tools/build-android-apk.sh
```

This configures a separate `build-android-release/`, refreshes MAVLink from the
same external repository as the desktop build, builds both ARM ABIs, aligns and
signs the Release APK, and verifies its signature, package/version, SDK levels,
ABIs, and absence of the debuggable flag. The APK, SHA256, signature details and
Android package metadata are written to `build-android-release/artifacts/`.

The launcher label is **THACO GroundControl** and the application ID is
`org.thaco.agridrone.qgroundcontrol`, so it can coexist with the stock QGC app.
The default persistent signing key and password file are created with restricted
permissions under `~/.local/share/thaco-groundcontrol/android-signing/`.
Keep this directory for future updates; signing material is outside the repository.

Override `BUILD_JOBS` (default 3), `BUILD_DIR`, `QT_BASE`, `QT_HOST_PATH`,
`ANDROID_SDK_ROOT`, `ANDROID_NDK_ROOT`, `JAVA_HOME`, or `GSTREAMER_ANDROID_ROOT`
to use other installations. `GSTREAMER_ANDROID_ROOT` must contain `arm64/` and
`armv7/` SDK directories. `ANDROID_ABIS` accepts a semicolon-separated ARM ABI
list; `ANDROID_PACKAGE_NAME` and `ANDROID_APP_NAME` override the application identity.
An existing signing key can be supplied through `ANDROID_KEYSTORE`,
`ANDROID_KEY_ALIAS`, and `ANDROID_KEYSTORE_PASSWORD_FILE`.

Transfer the APK to the MK15 and open it in the Android file manager to install.

## AI Vision

The Fly View AI Vision panel controls Bounding Box, Tracking, and Following.
Tracking and Following require Bounding Box. Changes send `CC_AI_VISION_CONTROL`
immediately and every second while a vehicle is connected. Click or tap inside
the displayed video to send one `MAV_CMD_CAMERA_TRACK_POINT` to Companion
component 191, with coordinates normalized to the displayed video area.
