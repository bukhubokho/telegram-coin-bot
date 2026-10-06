cmake_minimum_required(VERSION 3.22)
project(vanta_menu CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(vanta_menu SHARED
    src/main.cpp
    src/hook/elf_hook.cpp
    src/imgui/imgui.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/imgui_impl_android.cpp
    src/imgui/imgui_impl_opengl3.cpp
)

target_include_directories(vanta_menu PRIVATE
    src/
    src/imgui/
)

target_link_libraries(vanta_menu
    log
    android
    EGL
    GLESv2
)

target_compile_options(vanta_menu PRIVATE
    -O2
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
)

target_link_options(vanta_menu PRIVATE
    -Wl,--gc-sections
    -Wl,--strip-all
)

set_target_properties(vanta_menu PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/${ANDROID_ABI}"
)
