#!/usr/bin/env bash
# build.sh — build libvanta_menu.so cho arm64-v8a và armeabi-v7a
# yêu cầu: NDK r25+ đã set biến NDK hoặc ANDROID_NDK_HOME

NDK="${NDK:-${ANDROID_NDK_HOME}}"
if [ -z "$NDK" ]; then
    echo "[-] set NDK=/path/to/ndk trước khi chạy"
    exit 1
fi

TOOLCHAIN="$NDK/build/cmake/android.toolchain.cmake"
ABIS=("arm64-v8a" "armeabi-v7a")

for ABI in "${ABIS[@]}"; do
    echo "[*] building $ABI ..."
    mkdir -p "build/$ABI"
    cmake -S . -B "build/$ABI" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM=android-28 \
        -DCMAKE_BUILD_TYPE=Release \
        -G Ninja
    cmake --build "build/$ABI" --parallel
    echo "[+] out/$ABI/libvanta_menu.so"
done

echo "[+] done"
