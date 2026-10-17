# AgriDrone QGroundControl

Chạy các lệnh bên dưới từ thư mục gốc `THACOGroundControl/`.
Hướng dẫn dành cho máy build Linux hiện tại, đã cài CMake, Ninja, Python 3 và Qt **6.11.1**.
Phiên bản ứng dụng lấy từ tag Git `v5.1.4`, kèm mã commit của bản custom.

## 1. PC Linux — Debug để phát triển

```bash
./custom/tools/build-qgc.sh
./build/Debug/QGroundControl
```

Script này luôn build **Debug**, mặc định dùng 3 tác vụ song song và Qt tại
`~/Qt/6.11.1/gcc_64`. Có thể đổi số tác vụ, thư mục build hoặc đường dẫn Qt qua
`BUILD_JOBS`, `BUILD_DIR`, `QT_ROOT`. Ví dụ:

```bash
BUILD_JOBS=2 ./custom/tools/build-qgc.sh
```

## 2. PC Linux — Release AppImage để phân phối

```bash
python3 tools/configure.py -B build-linux-release --release --stable \
  --qt-root "$HOME/Qt/6.11.1/gcc_64" -- \
  -DQGC_USE_CACHE=OFF -DQGC_LINK_PARALLEL_LEVEL=1

cmake --build build-linux-release --config Release --parallel 3
cmake --install build-linux-release --config Release
```

Bước `cmake --install` gom thư viện vào AppDir và tạo file:
`build-linux-release/QGroundControl-x86_64.AppImage`.
Nếu chỉ sửa mã nguồn và giữ nguyên cấu hình, chạy lại hai lệnh build và install.

Chạy AppImage:

```bash
chmod +x build-linux-release/QGroundControl-x86_64.AppImage
./build-linux-release/QGroundControl-x86_64.AppImage
```

Bản build trên máy hiện tại dành cho **Ubuntu 24.04/26.04 x86_64**, không tương thích
Ubuntu 22.04. Linux dùng GStreamer hệ thống; máy hiện tại có bản **1.24.2**.

## 3. Android — Release APK cho SIYI MK15

MK15 dùng Android 9. APK mặc định chứa cả **ARM64 và ARMv7**.
Bộ công cụ cần có:

| Thành phần | Phiên bản |
| --- | --- |
| Qt host và hai bộ Android | 6.11.1: `gcc_64`, `android_arm64_v8a`, `android_armv7` |
| JDK | 21 |
| Android SDK / Build Tools | 36 / 36.0.0 |
| Android NDK | r27c / 27.2.12479018 |
| GStreamer Android | 1.28.4, có cả thư mục `arm64/` và `armv7/` |
| Android tối thiểu | 9 / API 28 |

Trên máy hiện tại, Qt ở `~/Qt/6.11.1`, SDK ở `~/Android/Sdk`,
JDK và GStreamer ở `~/.local/share/thaco-android-build/`.

Build và ký APK:

```bash
CCACHE_DISABLE=1 ./custom/tools/build-android-apk.sh
```

File cài đặt nằm tại:
`build-android-release/artifacts/THACOGroundControl-<version>-<commit>-android-arm.apk`.
Thư mục này cũng chứa SHA256, thông tin chữ ký và thông tin gói APK đã kiểm tra.
Chép APK lên MK15 rồi mở bằng trình quản lý file để cài.

Ứng dụng tên **THACO GroundControl**, mã gói `org.thaco.agridrone.qgroundcontrol`,
có thể cài cùng QGC gốc. **Giữ lại thư mục khóa ký**
`~/.local/share/thaco-groundcontrol/android-signing/` để các bản APK sau cài cập nhật được.

Có thể đổi số tác vụ qua `BUILD_JOBS`; đổi bộ công cụ qua `QT_BASE`, `QT_HOST_PATH`,
`ANDROID_SDK_ROOT`, `ANDROID_NDK_ROOT`, `JAVA_HOME`, `GSTREAMER_ANDROID_ROOT`.
Để dùng khóa ký khác, đặt `ANDROID_KEYSTORE`, `ANDROID_KEY_ALIAS` và
`ANDROID_KEYSTORE_PASSWORD_FILE`.

## MAVLink và phiên bản

Mỗi lần configure, build tự lấy commit mới nhất của nhánh `main` từ
[agridrone-mavlink](https://github.com/ThongTruong24/agridrone-mavlink).
Cần Internet; không sửa định nghĩa hoặc header MAVLink trong QGC.
Chỉ chạy `cmake --build` sẽ không cập nhật MAVLink: muốn cập nhật, chạy lại bước configure
hoặc script build tương ứng. File `custom/mavlink-revision.txt` chỉ ghi revision cũ để tham khảo.
Giữ tag `v5.1.4` khi chuyển repo sang máy khác để lấy đúng số phiên bản.

## AI Vision

Giao diện bay hỗ trợ Bounding Box, Tracking và Following. Bật Bounding Box trước khi dùng
Tracking/Following; chạm một điểm trên video để gửi tọa độ bám mục tiêu.

`CC_AI_VISION_CONTROL` (42015) gửi ba cờ qua link nhận heartbeat Companion (component 191),
cùng link dùng để đọc/ghi tham số Pi. Luồng gửi không phụ thuộc FC hoặc Vehicle và không
chọn link UDP connected đầu tiên hay link chính của FC. QGC gửi khi đổi cờ và định kỳ 1 Hz;
trước khi biết link Companion, các cờ chỉ được giữ trong ứng dụng. Khi link Companion kết nối
lại và có heartbeat mới, QGC tiếp tục gửi trạng thái hiện tại. Message và topic ROS2
`/cc/ai_vision_control` trên Pi giữ nguyên.

Kiểm tra trên MK15 với link UDP tới IP Ethernet của Pi (ví dụ `192.168.144.141:14550`):
không nối FC, giữ thêm một link UDP khác đang mở, bật/tắt ba cờ và kiểm tra topic ROS2 trên Pi.
Lặp lại khi nối FC; cả hai trường hợp phải đi qua link Companion.

## Tài liệu chính thức

- [Build QGroundControl](https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/getting_started/)
- [Triển khai Qt trên Linux](https://doc.qt.io/qt-6.11/linux-deployment.html)
- [Chuẩn bị môi trường Android](https://doc.qt.io/qt-6.11/android-configure-dev-environment.html)
- [Build APK bằng CMake](https://doc.qt.io/qt-6.11/android-building-projects-from-commandline.html)

Phiên bản toolchain được cấu hình trong [build-config.json](../.github/build-config.json).
