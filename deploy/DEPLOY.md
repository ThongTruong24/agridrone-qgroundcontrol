# Build và deploy AgriDroneGCS (Linux, Windows, Android)

Tài liệu này hướng dẫn build và đóng gói ứng dụng AgriDroneGCS (app `com.agridrone.gcs`, fork của QGroundControl 5.1.x) cho ba nền tảng: Linux (AppImage, .deb), Windows (installer NSIS) và Android (APK).

Mọi lệnh chạy từ thư mục `THACOGroundControl/`.

## 1. Yêu cầu chung

| Thành phần | Phiên bản / ghi chú |
|---|---|
| Qt | **6.11.1** (`.github/build-config.json`, mục `qt.version`) |
| CMake | ≥ 3.25 |
| Ninja | Generator mặc định của tất cả preset |
| Python | 3.x (script trong `tools/` và `custom/tools/`) |
| Git submodule | `git submodule update --init --recursive` (hoặc `just submodules`) |
| MAVLink của THACO | Build phải nằm trong meta-repo `THACO_Drone`, hoặc đặt biến `THACO_ROOT` trỏ tới checkout đó. Nếu thiếu, CMake dừng với lỗi "Build this custom QGC inside THACO_Drone". |

**Lưu ý trước khi build:**

- Lệnh `configure.py` dùng thư mục build trong repo (`build/`, `build-release/`). Các preset Windows/Android/Linux đặt thư mục build ở `../build/<preset>`, tức nằm trong `THACO_Drone/build/`, ngoài `THACOGroundControl/`.
- Cài Qt bằng `tools/setup/install_qt.py` (dùng aqtinstall). Xem đầy đủ tham số bằng `python3 tools/setup/install_qt.py install --help`.
- Cấu hình chung của custom build nằm ở `custom/cmake/CustomOverrides.cmake`: tên app `AgriDroneGCS` và package `com.agridrone.gcs`. Settings lưu dưới tên `QGroundControl` không được chuyển sang bản này (xem ADR-0008).

## 2. Linux

### 2.1 Chuẩn bị

```bash
just deps                      # cài dependency hệ thống (Debian/Ubuntu, cần sudo)
# hoặc:
python3 tools/setup/install_dependencies --platform debian
just submodules
```

Cài Qt 6.11.1 cho desktop (kit `gcc_64`) rồi trỏ `QT_DIR` hoặc `--qt-root` tới đó (mặc định `~/Qt/6.11.1/gcc_64`).

### 2.2 Build Release

```bash
python3 tools/configure.py -B build-release --release --qt-root ~/Qt/6.11.1/gcc_64
cmake --build build-release --config Release --parallel
```

### 2.3 Đóng gói

Mặc định (`QGC_CREATE_APPIMAGE=ON`), `cmake --install` tạo **AppImage** và lưu tại thư mục build:

```bash
cmake --install build-release
# kết quả: build-release/AgriDroneGCS-x86_64.AppImage
```

Bước này tải `linuxdeploy` và `appimagetool` từ GitHub, nên cần internet.

Muốn tạo **.deb** thay vì (hoặc cùng) AppImage: configure lại với `QGC_CPACK_GENERATOR=DEB` rồi build target `qgc-package`:

```bash
python3 tools/configure.py -B build-release --release --qt-root ~/Qt/6.11.1/gcc_64 -DQGC_CPACK_GENERATOR=DEB
cmake --build build-release --target qgc-package
```

Host cần `dpkg-dev` / `dpkg-shlibdeps` để cpack tạo .deb. Với Fedora/Arch, dùng `RPM` hoặc bỏ qua .deb.

### 2.4 Chạy

```bash
./build-release/AgriDroneGCS-x86_64.AppImage
# hoặc binary trong thư mục build
```

Trên WSL, render bằng phần mềm:

```bash
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe ./build/Debug/AgriDroneGCS
```

Không đánh giá chất lượng video trên WSL: đồng hồ WSL2 chạy chậm, gây rớt gói RTSP. Dùng QGC native trên Windows, VLC hoặc tay cầm SIYI.

## 3. Windows

### 3.1 Chuẩn bị

- **Visual Studio** có workload C++ (MSVC). Preset dùng Ninja Multi-Config.
- **Qt 6.11.1 MSVC kit** (ví dụ `msvc2022_64`). Đặt biến môi trường `QT_ROOT_DIR` trỏ tới thư mục Qt đó. Toolchain của preset đọc `$env:QT_ROOT_DIR\lib\cmake\Qt6\qt.toolchain.cmake`.
- **NSIS** (`makensis`) để tạo installer `.exe`. CMake tự tìm `makensis`.
- **sccache** (preset Windows kế thừa `sccache`, `QGC_USE_CACHE=ON`). Nếu không dùng cache, tắt bằng `-DQGC_USE_CACHE=OFF`.
- GStreamer 1.28.4 cho Windows được CMake tự tải (`cmake/GStreamer/Download.cmake`). Muốn dùng SDK đã cài sẵn, đặt `GSTREAMER_ROOT`.

### 3.2 Build bằng CMake preset (khuyến nghị)

**Chạy trên Windows gốc, không chạy trong WSL.** Preset `Windows` có điều kiện `hostSystemName == Windows`, nên từ WSL (Linux) CMake báo "Cannot use disabled configure preset". Clone repo vào ổ Windows (ví dụ `C:\src\THACO_Drone`), không dùng `~/THACO_Drone` trong WSL.

Nếu `cmake --preset Windows` báo "No such configure preset", `CMakePresets.json` đang là bản cũ chỉ có preset `default`. File này được sinh từ template: copy `CMakePresets.json.template` đè lên `CMakePresets.json` (template include `cmake/presets/*.json`).

Từ Developer PowerShell hoặc terminal có môi trường MSVC:

```powershell
cmake --preset Windows                 # configure, build/Windows (Release)
cmake --build --preset Windows         # build
cmake --workflow --preset Windows      # configure + build + test + package một lệnh
```

Có sẵn các preset `Windows-debug`, `Windows-arm64`, `Windows-arm64-debug`. Thư mục build là `..\build\Windows` (trong `THACO_Drone\build\`).

### 3.3 Build bằng `configure.py` (cách khác)

```powershell
python tools\configure.py -B build -t Release --qt-root C:\Qt\6.11.1\msvc2022_64
cmake --build build --config Release --parallel
cmake --install build --config Release
```

`cmake --install` tạo `AgriDroneGCS-installer-<arch>.exe` trong thư mục build.

### 3.4 Chạy và kiểm thử

- Chạy installer `.exe` rồi mở AgriDroneGCS từ Start Menu, hoặc chạy `..\build\Windows\Release\AgriDroneGCS.exe` (cần Qt DLL cạnh exe; cách an toàn là dùng installer).
- Test: `ctest --preset Windows` (preset này chạy build Release; `Windows-debug` chạy build Debug). Test cụ thể xem `CLAUDE.md` mục test cho binary `--unittest`.

## 4. Android

### 4.1 Chuẩn bị

Theo `.github/build-config.json`:

| Thành phần | Giá trị |
|---|---|
| Android platform (compile/target SDK) | **36** |
| Min SDK | **28** |
| Build tools | **36.0.0** |
| NDK | **r27c** (`27.2.12479018`) |
| JDK | **21** |
| ABI | `arm64-v8a` (preset cố định) |
| Qt | 6.11.1, kit Android `android_arm64_v8a`, cộng với Qt desktop làm host (`QT_HOST_PATH`) |

Biến môi trường mà preset Android đọc:

- `QT_ROOT_DIR`: Qt Android kit (qua toolchain Qt).
- `QT_HOST_PATH`: Qt desktop cùng phiên bản.
- `ANDROID_NDK`: đường dẫn NDK.
- `ANDROID_PLATFORM`: API level, ví dụ `android-36`.

Tự động có khi build: GStreamer Android 1.28.4 (tải về) và OpenSSL 3.5.6 (build từ source, `cmake/modules/AndroidOpenSSL.cmake`).

### 4.2 Build

```bash
cmake --preset Android                 # Release, build/Android
cmake --build --preset Android
cmake --workflow --preset Android      # configure + build + package
```

Debug: `cmake --preset Android-debug` rồi `cmake --build --preset Android-debug`.

Tìm file APK sau build:

```bash
find ../build/Android -name '*.apk'
```

Đường dẫn chính xác phụ thuộc androiddeployqt; chưa kiểm chứng trong tài liệu này.

### 4.3 Ký APK (release)

- Cờ CMake `QT_ANDROID_SIGN_APK` (mặc định `OFF`) bật ký APK. `QT_ANDROID_SIGN_AAB` bật ký AAB.
- Repo có `deploy/android/android_release.keystore`, nhưng file này nằm trong `.gitignore`. **Không commit keystore hay mật khẩu.** Mật khẩu phải được truyền ngoài repo.
- Chưa tìm thấy trong CMake cấu hình đường dẫn keystore và alias. Cần kiểm tra cách truyền thông số này (qua `androiddeployqt` hoặc biến CMake) trước khi build release đã ký.

### 4.4 Cài lên thiết bị

```bash
adb install -r path/to/AgriDroneGCS.apk
```

Kiểm tra trước khi phát hành: `android/AndroidManifest.xml` có placeholder `-- %%INSERT_VERSION_CODE%% --` và `-- %%INSERT_VERSION_NAME%% --`. Chưa tìm thấy bước nào trong CMake thay thế các placeholder này. Nếu không được thay, versionCode/versionName sẽ không hợp lệ.

## 5. Kiểm tra sau build

Theo quy trình bug-fix/CI của dự án (`CLAUDE.md`, `AGENTS.md`):

```bash
just check                              # lint + test (Linux)
ctest --test-dir build -L "Unit|Integration" -LE "Flaky|Network"
```

Với bản triển khai lên drone, chạy thêm `gcs_bench.py` theo `CLAUDE.md` của repo gốc (`--mode sitl`, và `--mode bench` khi deploy).

## 6. Bảng tóm tắt

| Nền tảng | Preset / lệnh | Đầu ra | Ghi chú |
|---|---|---|---|
| Linux | `configure.py --release` + `cmake --install` | `AgriDroneGCS-x86_64.AppImage` | Cần internet để tải linuxdeploy |
| Linux (.deb) | `-DQGC_CPACK_GENERATOR=DEB` + `--target qgc-package` | `.deb` | Cần `dpkg-dev` |
| Windows | `cmake --workflow --preset Windows` | installer `.exe` (NSIS) | Cần MSVC, Qt MSVC, NSIS |
| Android | `cmake --workflow --preset Android` | `.apk` | Cần NDK r27c, JDK 21, Qt Android |

## 7. Việc chưa kiểm chứng

- Chưa chạy build Windows và Android trên máy này. Các bước ở mục 3 và 4 dựa trên đọc `cmake/presets/*.json`, `cmake/install/*.cmake` và `build-config.json`.
- Đường dẫn APK và tên file installer Windows chưa xác minh bằng build thật.
- Cách truyền keystore khi ký APK chưa xác minh.
- Placeholder version trong `AndroidManifest.xml` cần kiểm tra trước khi phát hành.
