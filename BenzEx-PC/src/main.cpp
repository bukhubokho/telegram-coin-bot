// language: C++17, file: main.cpp, target: Windows 11, MSVC/MinGW
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "FONTS/DEFAULT.h"
#include "FONTS/fa_solid.h"
#include "Includes/FontAwesomeIcons.h"

#include "KeyAuthUI.h"
#include "UI/MenuUI.h"

#include <GLFW/glfw3.h>
#include <cstdio>

static void GlfwErrorCb(int error, const char* description) {
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

int main() {
    glfwSetErrorCallback(GlfwErrorCb);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    const int winW = 500, winH = 360;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    int posX = mode ? (mode->width  - winW) / 2 : 200;
    int posY = mode ? (mode->height - winH) / 2 : 200;

    GLFWwindow* window = glfwCreateWindow(winW, winH, "BenzEx", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwSetWindowPos(window, posX, posY);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImFontConfig fontCfg;
    fontCfg.SizePixels = 18.0f;
    fontCfg.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF((void*)Custom3, sizeof(Custom3), 18.0f, &fontCfg);

    ImFontConfig iconCfg;
    iconCfg.MergeMode = true;
    iconCfg.PixelSnapH = true;
    iconCfg.FontDataOwnedByAtlas = false;
    static const ImWchar icon_ranges[] = { 0xe000, 0xf8ff, 0 };
    io.Fonts->AddFontFromMemoryTTF((void*)fa_solid_900_ttf, fa_solid_900_ttf_len,
        18.0f, &iconCfg, icon_ranges);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    KeyAuthUI::Init();
    MenuUI::Init();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        // Navy background — matches KeyAuthUI/MenuUI COL_BG (15, 20, 33)
        glClearColor(0.059f, 0.078f, 0.129f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (KeyAuthUI::State() != KeyAuthUI::AuthState::Success) {
            KeyAuthUI::Draw();
        } else {
            static bool resized = false;
            if (!resized) {
                glfwSetWindowSize(window, 700, 500);
                const GLFWvidmode* m = glfwGetVideoMode(glfwGetPrimaryMonitor());
                if (m) glfwSetWindowPos(window, (m->width - 700) / 2, (m->height - 500) / 2);
                resized = true;
            }
            MenuUI::Draw();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    MenuUI::Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
