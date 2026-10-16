# Build và triển khai THACO GroundControl

Hướng dẫn build của checkout này nằm tại [custom/README.md](../custom/README.md),
bao gồm Linux Debug, Linux Release AppImage và Android Release APK cho SIYI MK15.
Dùng các lệnh và đường dẫn toolchain trong tài liệu đó.

Luồng build giữ nguyên binary Linux `QGroundControl` và APK Android
`THACO GroundControl` / `org.thaco.agridrone.qgroundcontrol`.
Script Android giữ cách build ARM64 + ARMv7, ký PKCS12 và kiểm tra APK của local.

Mỗi lần configure, `custom/cmake/CustomOverrides.cmake` lấy SHA mới nhất của
`https://github.com/ThongTruong24/agridrone-mavlink` nhánh `main` và CPM sinh header
trong thư mục build. Checkout này không cần `THACO_ROOT` hay helper MAVLink ở repo cha.
