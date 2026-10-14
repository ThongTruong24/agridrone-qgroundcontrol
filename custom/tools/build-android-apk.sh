#!/usr/bin/env bash
set -euo pipefail

QGC_SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
QGC_SOURCE_ROOT="$(git -C "$QGC_SCRIPT_DIR" rev-parse --show-toplevel)"
mapfile -t QGC_CONFIG < <(python3 - "$QGC_SOURCE_ROOT/.github/build-config.json" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as source:
    config = json.load(source)
for value in (
    config["qt"]["version"], config["android"]["ndk_full_version"],
    config["android"]["build_tools"], config["gstreamer"]["version"]["android"],
    config["android"]["platform"], config["android"]["min_sdk"],
):
    print(value)
PY
)

QGC_QT_VERSION="${QGC_CONFIG[0]}"
QGC_BUILD_DIR="${BUILD_DIR:-$QGC_SOURCE_ROOT/build-android-release}"
QGC_QT_BASE="${QT_BASE:-$HOME/Qt/$QGC_QT_VERSION}"
QGC_HOST_QT="${QT_HOST_PATH:-$QGC_QT_BASE/gcc_64}"
QGC_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
QGC_NDK_ROOT="${ANDROID_NDK_ROOT:-$QGC_SDK_ROOT/ndk/${QGC_CONFIG[1]}}"
QGC_JAVA_ROOT="${JAVA_HOME:-$HOME/.local/share/thaco-android-build/jdk/current}"
QGC_GST_ROOT="${GSTREAMER_ANDROID_ROOT:-$HOME/.local/share/thaco-android-build/gstreamer/${QGC_CONFIG[3]}}"
QGC_ABIS="${ANDROID_ABIS:-arm64-v8a;armeabi-v7a}"
QGC_PACKAGE="${ANDROID_PACKAGE_NAME:-org.thaco.agridrone.qgroundcontrol}"
QGC_APP_LABEL="${ANDROID_APP_NAME:-THACO GroundControl}"
QGC_SIGNING_DIR="${SIGNING_DIR:-$HOME/.local/share/thaco-groundcontrol/android-signing}"
QGC_KEYSTORE="${ANDROID_KEYSTORE:-$QGC_SIGNING_DIR/release.p12}"
QGC_PASS_FILE="${ANDROID_KEYSTORE_PASSWORD_FILE:-$QGC_SIGNING_DIR/password}"
QGC_KEY_ALIAS="${ANDROID_KEY_ALIAS:-thaco-groundcontrol}"
QGC_BUILD_TOOLS="$QGC_SDK_ROOT/build-tools/${QGC_CONFIG[2]}"
BUILD_JOBS="${BUILD_JOBS:-3}"

if [[ ! "$BUILD_JOBS" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERROR: BUILD_JOBS must be a positive integer" >&2
    exit 1
fi
if (( BUILD_JOBS > $(nproc) )); then BUILD_JOBS="$(nproc)"; fi

IFS=';' read -r -a QGC_ABI_LIST <<< "$QGC_ABIS"
case "${QGC_ABI_LIST[0]}" in
    arm64-v8a) QGC_QT_ROOT="$QGC_QT_BASE/android_arm64_v8a" ;;
    armeabi-v7a) QGC_QT_ROOT="$QGC_QT_BASE/android_armv7" ;;
    *) echo "ERROR: Android ABI must be arm64-v8a or armeabi-v7a" >&2; exit 1 ;;
esac
for QGC_ABI in "${QGC_ABI_LIST[@]}"; do
    case "$QGC_ABI" in
        arm64-v8a) QGC_KIT_DIR="$QGC_QT_BASE/android_arm64_v8a"; QGC_GST_ABI=arm64 ;;
        armeabi-v7a) QGC_KIT_DIR="$QGC_QT_BASE/android_armv7"; QGC_GST_ABI=armv7 ;;
        *) echo "ERROR: Unsupported Android ABI: $QGC_ABI" >&2; exit 1 ;;
    esac
    if [[ ! -f "$QGC_KIT_DIR/lib/cmake/Qt6/qt.toolchain.cmake" || ! -d "$QGC_GST_ROOT/$QGC_GST_ABI" ]]; then
        echo "ERROR: Install Qt $QGC_QT_VERSION and GStreamer ${QGC_CONFIG[3]} for $QGC_ABI" >&2
        exit 1
    fi
done
for QGC_TOOL in "$QGC_QT_ROOT/bin/qt-cmake" "$QGC_HOST_QT/libexec/moc" "$QGC_JAVA_ROOT/bin/java" \
    "$QGC_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/clang" \
    "$QGC_BUILD_TOOLS/apksigner" "$QGC_BUILD_TOOLS/zipalign" "$QGC_BUILD_TOOLS/aapt"; do
    if [[ ! -x "$QGC_TOOL" ]]; then
        echo "ERROR: Required build tool not found: $QGC_TOOL" >&2
        exit 1
    fi
done

QGC_VERSION="$(git -C "$QGC_SOURCE_ROOT" describe --tags --abbrev=0)"
QGC_VERSION="${QGC_VERSION#v}"
QGC_COMMIT="$(git -C "$QGC_SOURCE_ROOT" rev-parse --short HEAD)"
export JAVA_HOME="$QGC_JAVA_ROOT"
export PATH="$JAVA_HOME/bin:$PATH"
export CMAKE_BUILD_PARALLEL_LEVEL="$BUILD_JOBS"

"$QGC_QT_ROOT/bin/qt-cmake" -S "$QGC_SOURCE_ROOT" -B "$QGC_BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DQGC_STABLE_BUILD=ON -DQGC_BUILD_TESTING=OFF \
    -DQGC_DEBUG_QML=OFF -DQGC_USE_CACHE=OFF -DQGC_LINK_PARALLEL_LEVEL=1 \
    -DANDROID_SDK_ROOT="$QGC_SDK_ROOT" -DANDROID_NDK_ROOT="$QGC_NDK_ROOT" \
    -DANDROID_ABI="${QGC_ABI_LIST[0]}" -DANDROID_PLATFORM="android-${QGC_CONFIG[5]}" \
    -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON -DQT_HOST_PATH="$QGC_HOST_QT" \
    -DQT_ANDROID_ABIS="$QGC_ABIS" -DQT_ANDROID_DEPLOYMENT_TYPE=Release \
    -DQT_ANDROID_SIGN_APK=OFF -DQGC_ANDROID_GSTREAMER_SDK_ROOT="$QGC_GST_ROOT" \
    -DQGC_PACKAGE_NAME="$QGC_PACKAGE" -DQGC_ANDROID_PACKAGE_NAME="$QGC_PACKAGE" \
    -DQGC_ANDROID_APP_NAME="$QGC_APP_LABEL" -DQGC_ANDROID_VERSION_NAME="$QGC_VERSION-$QGC_COMMIT" \
    -DPython3_EXECUTABLE="$QGC_SOURCE_ROOT/.venv/bin/python"

QGC_BUILD_COMMAND=(cmake --build "$QGC_BUILD_DIR" --config Release --target apk --parallel "$BUILD_JOBS")
if command -v ionice >/dev/null 2>&1; then
    nice -n 10 ionice -c2 -n7 "${QGC_BUILD_COMMAND[@]}"
else
    nice -n 10 "${QGC_BUILD_COMMAND[@]}"
fi

mapfile -t QGC_UNSIGNED_APKS < <(find "$QGC_BUILD_DIR" -path '*/outputs/apk/release/*-unsigned.apk' -type f)
if (( ${#QGC_UNSIGNED_APKS[@]} != 1 )); then
    echo "ERROR: Expected one unsigned Release APK, found ${#QGC_UNSIGNED_APKS[@]}" >&2
    exit 1
fi

if [[ ! -f "$QGC_KEYSTORE" ]]; then
    if [[ -n "${ANDROID_KEYSTORE:-}" ]]; then
        echo "ERROR: Supplied keystore does not exist: $QGC_KEYSTORE" >&2
        exit 1
    fi
    mkdir -p "$QGC_SIGNING_DIR"
    chmod 700 "$QGC_SIGNING_DIR"
    if [[ ! -f "$QGC_PASS_FILE" ]]; then
        (umask 077; python3 -c 'import secrets; print(secrets.token_hex(32))' > "$QGC_PASS_FILE")
    fi
    (umask 077; "$JAVA_HOME/bin/keytool" -genkeypair -noprompt -storetype PKCS12 \
        -keystore "$QGC_KEYSTORE" -storepass:file "$QGC_PASS_FILE" -keypass:file "$QGC_PASS_FILE" \
        -alias "$QGC_KEY_ALIAS" -keyalg RSA -keysize 3072 -validity 10000 \
        -dname "CN=THACO GroundControl, OU=AgriDrone, O=THACO, C=VN")
fi
if [[ ! -r "$QGC_PASS_FILE" ]]; then
    echo "ERROR: Keystore password file not readable: $QGC_PASS_FILE" >&2
    exit 1
fi

QGC_OUTPUT_DIR="$QGC_BUILD_DIR/artifacts"
mkdir -p "$QGC_OUTPUT_DIR"
QGC_APK="$QGC_OUTPUT_DIR/THACOGroundControl-$QGC_VERSION-$QGC_COMMIT-android-arm.apk"
QGC_ALIGNED_APK="$QGC_OUTPUT_DIR/aligned-unsigned.apk"
"$QGC_BUILD_TOOLS/zipalign" -P 16 -f 4 "${QGC_UNSIGNED_APKS[0]}" "$QGC_ALIGNED_APK"
"$QGC_BUILD_TOOLS/apksigner" sign --ks "$QGC_KEYSTORE" --ks-key-alias "$QGC_KEY_ALIAS" \
    --ks-pass "file:$QGC_PASS_FILE" --key-pass "file:$QGC_PASS_FILE" \
    --v4-signing-enabled false --out "$QGC_APK" "$QGC_ALIGNED_APK"
rm "$QGC_ALIGNED_APK"
"$QGC_BUILD_TOOLS/apksigner" verify --verbose --print-certs "$QGC_APK" > "$QGC_APK.signature.txt"
"$QGC_BUILD_TOOLS/zipalign" -c -P 16 4 "$QGC_APK"
"$QGC_BUILD_TOOLS/aapt" dump badging "$QGC_APK" > "$QGC_APK.badging.txt"
python3 - "$QGC_APK.badging.txt" "$QGC_PACKAGE" "$QGC_VERSION-$QGC_COMMIT" \
    "$QGC_ABIS" "${QGC_CONFIG[5]}" "${QGC_CONFIG[4]}" <<'PY'
import re
import shlex
import sys
from pathlib import Path
badging = Path(sys.argv[1]).read_text(encoding="utf-8")
package = re.search(r"^package: name='([^']+)' .*versionName='([^']+)'", badging, re.M)
if not package or package.groups() != (sys.argv[2], sys.argv[3]):
    raise SystemExit("ERROR: APK package/version does not match the requested release")
for field, expected in (("sdkVersion", sys.argv[5]), ("targetSdkVersion", sys.argv[6])):
    if f"{field}:'{expected}'" not in badging:
        raise SystemExit(f"ERROR: Unexpected APK {field}")
native = re.search(r"^native-code:(.*)$", badging, re.M)
if not native or set(shlex.split(native[1])) != set(sys.argv[4].split(";")):
    raise SystemExit("ERROR: APK does not contain the requested ABIs")
if "application-debuggable" in badging:
    raise SystemExit("ERROR: Release APK is debuggable")
PY
(cd "$QGC_OUTPUT_DIR"; sha256sum "$(basename "$QGC_APK")" > "$(basename "$QGC_APK").sha256")
echo "Signed APK: $QGC_APK"
