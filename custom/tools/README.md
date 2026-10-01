# Build QGC sau khi cập nhật MAVLink

`custom/cmake/CustomOverrides.cmake` tự lấy SHA mới nhất của nhánh `main` từ
`https://github.com/ThongTruong24/agridrone-mavlink.git` mỗi lần CMake configure.
CPM dùng SHA này để chọn cache nguồn, nên không phải sửa commit bằng tay hoặc xóa cache.
Nếu không lấy được phiên bản mới qua mạng, configure báo lỗi và script dừng trước bước build.

## Chạy hằng ngày

1. Commit và push thay đổi MAVLink lên `main` của repository trên.
2. Mở PowerShell tại thư mục QGC và chạy:

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\custom\tools\build-with-mavlink.ps1
   ```

Script chạy configure rồi build tăng dần trong
`build/Desktop_Qt_6_11_1_MSVC2022_64bit-Debug`. Nó giữ cấu hình CMake hiện có,
dùng CMake đã lưu trong cache và nạp môi trường Visual Studio x64 tương ứng.
Thư mục build phải được configure bằng kit phù hợp trong Qt Creator trước lần chạy đầu tiên.
Đường dẫn `-BuildDirectory` tương đối được tính từ thư mục QGC, không phụ thuộc thư mục terminal.

Có thể giới hạn số tác vụ build hoặc chọn thư mục build khác:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\custom\tools\build-with-mavlink.ps1 -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\custom\tools\build-with-mavlink.ps1 -BuildDirectory build/MyRelease -Configuration Release
```

Với Ninja một cấu hình, `-Configuration` phải khớp `CMAKE_BUILD_TYPE` của thư mục build.
Nếu bỏ qua tham số này, script dùng `CMAKE_BUILD_TYPE` trong cache; với generator nhiều cấu hình,
mặc định là `Debug`.

## Chạy bằng Qt Creator

Sau khi push MAVLink, chọn **Run CMake**, chờ configure thành công, rồi chọn **Build**.
Không cần **Clear CMake Configuration** hay **Clean**.
Chỉ chọn **Build** sẽ không kiểm tra phiên bản trên GitHub nếu CMake không chạy lại.

## Kiểm tra phiên bản được sử dụng

Log configure hiển thị `THACO MAVLink main: <SHA>`. Có thể kiểm tra cache sau khi build:

```powershell
Select-String -Path .\build\Desktop_Qt_6_11_1_MSVC2022_64bit-Debug\CMakeCache.txt -Pattern '^QGC_MAVLINK_GIT_TAG:'
```

Thay đổi mới chỉ có trên máy MAVLink mà chưa push lên `main` sẽ chưa được tải về.
Lần đầu dùng một SHA mới, CPM có thể tải nguồn và các submodule, rồi sinh lại header MAVLink;
các thư viện khác tiếp tục sử dụng cache hiện có.
Thêm message MAVLink mới cần phần xử lý/hiển thị tương ứng trong QGC nếu muốn sử dụng nó.
