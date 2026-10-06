cmake_minimum_required(VERSION 3.20)
project(BenzEx_PC CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/imgui")

set(SOURCES
    src/main.cpp
    src/KeyAuthUI.cpp
    src/UI/MenuUI.cpp
    ${IMGUI_DIR}/stb_image.cpp
)

add_executable(BenzEx WIN32 ${SOURCES})

target_include_directories(BenzEx PRIVATE
    src
    ${IMGUI_DIR}
)

target_link_libraries(BenzEx PRIVATE
    OpenGL::GL
    glfw
    imgui::imgui
    iphlpapi
    ws2_32
    winhttp
)

if(MSVC)
    target_compile_options(BenzEx PRIVATE /W3)
    set_target_properties(BenzEx PROPERTIES
        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )
endif()
