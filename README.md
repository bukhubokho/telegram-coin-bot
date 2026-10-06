# vanta_menu

ImGui cheat menu inject cho Android offline game — no root, dùng MT Manager.

## Cấu trúc

```
src/
  main.cpp              — EGL hook + ImGui render + JNI
  hook/
    elf_hook.h/.cpp     — PLT/GOT hook không cần root
  imgui/                — paste ImGui source vào đây
java/
  com/vanta/Bridge.java — JNI bridge nhận touch từ smali
CMakeLists.txt
build.sh
```

## Bước 1 — Thêm ImGui

```bash
git clone https://github.com/ocornut/imgui --depth=1 _imgui
cp _imgui/imgui*.{h,cpp} src/imgui/
cp _imgui/backends/imgui_impl_android.{h,cpp} src/imgui/
cp _imgui/backends/imgui_impl_opengl3.{h,cpp} src/imgui/
cp _imgui/backends/imgui_impl_opengl3_loader.h src/imgui/
rm -rf _imgui
```

## Bước 2 — Build

```bash
export NDK=/path/to/ndk
chmod +x build.sh && ./build.sh
# output: out/arm64-v8a/libvanta_menu.so
#         out/armeabi-v7a/libvanta_menu.so
```

## Bước 3 — MT Manager inject

1. Mở APK → `lib/arm64-v8a/` → thêm `libvanta_menu.so`
2. Tìm Activity chính trong smali → patch `onTouchEvent` gọi `Bridge.onTouch(e)`
3. Merge `Bridge.dex` vào `classes.dex`
4. Sign → install
