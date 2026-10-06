# AgriDrone QGroundControl

## Build

```bash
./custom/tools/build-qgc.sh
```

Script sẽ:

```text
1. Kiểm tra agridrone-mavlink/main
2. Nếu MAVLink có thay đổi thì chạy lại CMake configure
3. Build QGroundControl
```

## Clean build

```bash
rm -rf build && ./custom/tools/build-qgc.sh
```

## Run QGC

```bash
./build/Debug/QGroundControl
```

## Build và chạy

```bash
./custom/tools/build-qgc.sh && ./build/Debug/QGroundControl
```
