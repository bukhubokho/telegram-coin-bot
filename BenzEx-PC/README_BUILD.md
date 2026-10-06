# BenzEx PC — Build Guide

## Requirements
- Windows 10/11, MSVC 2022 (or MinGW-w64 with GCC 12+)
- CMake ≥ 3.20
- vcpkg

## Steps

### 1. Install vcpkg dependencies
```bash
vcpkg install glfw3:x64-windows
vcpkg install glad:x64-windows   # optional – see note below
```

### 2. Add ImGui backends
Download these two files from the ImGui repo (same version as the files in `/imgui/`):
- `imgui_impl_glfw.h` / `imgui_impl_glfw.cpp`
- `imgui_impl_opengl3.h` / `imgui_impl_opengl3.cpp`

Place them in `imgui/backends/`.

### 3. glad (OpenGL loader)
Either:
- **Option A** – generate from https://glad.dav1d.de (GL 3.3 core, C/C++) and put `glad.c` + `include/glad/glad.h` in the project.
- **Option B** – replace `#include <glad/glad.h>` in `src/UI/MenuUI.cpp` with `#include <GL/gl.h>` (Windows SDK GL), then remove the glad library from CMakeLists.

### 4. Configure & build
```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

Output: `build/Release/BenzEx.exe`

---

## Project layout
```
BenzEx-PC/
├── CMakeLists.txt
├── src/
│   ├── main.cpp            ← GLFW + OpenGL3 host, state machine
│   ├── KeyAuthUI.h/.cpp    ← Key activation screen
│   ├── UI/
│   │   ├── MenuUI.h
│   │   └── MenuUI.cpp      ← Original menu (only GL header patched)
│   └── Includes/
│       ├── FontAwesomeIcons.h
│       ├── MenuIconData.h  ← Replace with your real icon PNG
│       └── Logger.h
└── imgui/
    ├── imgui*.cpp / *.h
    ├── stb_image.*
    ├── FONTS/              ← DEFAULT.h, fa_solid.h
    └── backends/           ← imgui_impl_glfw.*, imgui_impl_opengl3.*
```

---

## Key Activation flow
1. Window opens → shows key activation screen
2. User types their key code → clicks **Activate**
3. App reads HWID (CPU + volume serial + MAC → 16-char hex)
4. `POST https://telegram-coin-bot-1.onrender.com/api/activate`  
   Body: `{"key":"<entered key>","hwid":"<hwid>"}`
5. On `"ok":true` → slides into main menu
6. On failure → shows server error message, lets user retry

---

## Replacing the icon
Convert your PNG:
```bash
xxd -i icon.png | sed 's/unsigned char/static const uint8_t/; s/unsigned int/static const size_t/' > src/Includes/MenuIconData.h
# Then rename the array vars to: menu_icon_png / menu_icon_png_len
```
