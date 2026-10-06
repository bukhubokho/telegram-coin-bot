// language: C++17, file: main.cpp, target: Android ARM64, NDK r25+
// init_array constructor → hook eglSwapBuffers → ImGui overlay + touch
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <mutex>
#include <cstring>
#include "hook/elf_hook.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_android.h"
#include "imgui/imgui_impl_opengl3.h"

#define TAG "vanta"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using eglSwapBuffers_t = EGLBoolean(*)(EGLDisplay, EGLSurface);
static eglSwapBuffers_t o_eglSwapBuffers = nullptr;

static bool        g_ImGuiInit = false;
static bool        g_MenuOpen  = true;
static std::mutex  g_Mtx;
static int         g_W = 0, g_H = 0;

// --- cheat toggles ---
static bool  b_GodMode   = false;
static bool  b_InfAmmo   = false;
static float f_Speed     = 1.0f;
static bool  b_ESP       = false;
static bool  b_NoRecoil  = false;

static void imgui_init(EGLDisplay dpy, EGLSurface surf) {
    EGLint w = 0, h = 0;
    eglQuerySurface(dpy, surf, EGL_WIDTH,  &w);
    eglQuerySurface(dpy, surf, EGL_HEIGHT, &h);
    if (w <= 0 || h <= 0) return;
    g_W = w; g_H = h;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize       = { (float)w, (float)h };
    io.FontGlobalScale   = w >= 1080 ? 2.2f : 1.6f;
    io.IniFilename       = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 10.f;
    s.FrameRounding  = 5.f;
    s.GrabRounding   = 5.f;
    s.WindowPadding  = { 14, 12 };
    s.Colors[ImGuiCol_WindowBg]      = { 0.05f, 0.05f, 0.08f, 0.92f };
    s.Colors[ImGuiCol_TitleBgActive] = { 0.12f, 0.40f, 0.65f, 1.00f };
    s.Colors[ImGuiCol_CheckMark]     = { 0.30f, 0.80f, 1.00f, 1.00f };
    s.Colors[ImGuiCol_SliderGrab]    = { 0.30f, 0.80f, 1.00f, 1.00f };
    s.Colors[ImGuiCol_Button]        = { 0.15f, 0.45f, 0.70f, 1.00f };

    ImGui_ImplAndroid_Init(nullptr);
    ImGui_ImplOpenGL3_Init("#version 100");

    g_ImGuiInit = true;
    LOGI("ImGui ready %dx%d", w, h);
}

static void render_menu() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    if (g_MenuOpen) {
        float sw = (float)g_W, sh = (float)g_H;
        ImGui::SetNextWindowPos({ sw * 0.04f, sh * 0.04f }, ImGuiCond_Once);
        ImGui::SetNextWindowSize({ sw * 0.52f, sh * 0.58f }, ImGuiCond_Once);
        ImGui::Begin("##vanta_main", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

        // header
        ImGui::TextColored({ 0.3f, 0.8f, 1.f, 1.f }, "VANTA MENU");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 38);
        if (ImGui::Button("X##close", { 34, 26 })) g_MenuOpen = false;
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("God Mode",       &b_GodMode);
            ImGui::Checkbox("Infinite Ammo",  &b_InfAmmo);
            ImGui::Checkbox("No Recoil",      &b_NoRecoil);
            ImGui::SliderFloat("Speed##spd", &f_Speed, 0.5f, 6.0f, "%.1fx");
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Visual")) {
            ImGui::Checkbox("Wallhack / ESP", &b_ESP);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("v1.0  |  tap [V] to reopen");
        ImGui::End();
    } else {
        // floating reopen button
        ImGui::SetNextWindowPos({ 8.f, 8.f }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ 58.f, 36.f }, ImGuiCond_Always);
        ImGui::Begin("##vanta_btn", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize  |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
        if (ImGui::Button("[V]", { 54.f, 30.f })) g_MenuOpen = true;
        ImGui::End();
    }

    ImGui::Render();
    glViewport(0, 0, g_W, g_H);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// --- public: nhận touch từ JNI ---
extern "C" void vanta_touch(float x, float y, int action) {
    if (!g_ImGuiInit) return;
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
    // action: 0=DOWN, 1=UP, 2=MOVE
    io.AddMouseButtonEvent(0, action == 0 || action == 2);
}

// --- hook ---
static EGLBoolean hk_eglSwapBuffers(EGLDisplay dpy, EGLSurface surf) {
    std::lock_guard<std::mutex> lk(g_Mtx);
    if (!g_ImGuiInit) imgui_init(dpy, surf);
    if (g_ImGuiInit)  render_menu();
    return o_eglSwapBuffers(dpy, surf);
}

__attribute__((constructor))
static void vanta_init() {
    LOGI("vanta_init — hooking eglSwapBuffers");

    void* orig = elf_hook("libEGL.so", "eglSwapBuffers", (void*)hk_eglSwapBuffers);
    if (!orig)
        orig = elf_hook("libGLESv2.so", "eglSwapBuffers", (void*)hk_eglSwapBuffers);

    o_eglSwapBuffers = orig
        ? (eglSwapBuffers_t)orig
        : (eglSwapBuffers_t)dlsym(RTLD_DEFAULT, "eglSwapBuffers");

    LOGI("o_eglSwapBuffers = %p", (void*)o_eglSwapBuffers);
}

// --- JNI bridge ---
extern "C" JNIEXPORT void JNICALL
Java_com_vanta_Bridge_nativeTouch(JNIEnv*, jclass, jfloat x, jfloat y, jint action) {
    vanta_touch(x, y, action);
}
