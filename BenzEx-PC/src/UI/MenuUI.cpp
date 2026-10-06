// language: C++17, file: MenuUI.cpp, target: Windows 11, MSVC
#include "MenuUI.h"
#include "imgui_internal.h"
#include "Includes/FontAwesomeIcons.h"
#include "Includes/MenuIconData.h"
#include "stb_image.h"
#include <glad/glad.h>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace MenuUI {

Config g_Config;

static GLuint s_iconTex  = 0;
static bool   s_iconDone = false;

static void LoadIcon() {
    if (s_iconDone) return;
    s_iconDone = true;
    int w, h, c;
    unsigned char* px = stbi_load_from_memory(menu_icon_png, (int)menu_icon_png_len, &w, &h, &c, 4);
    if (!px) return;
    glGenTextures(1, &s_iconTex);
    glBindTexture(GL_TEXTURE_2D, s_iconTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(px);
}

// ── Palette ───────────────────────────────────────────────────────────────
#define C_BG       IM_COL32( 12,  17,  28, 255)
#define C_SIDE     IM_COL32(  8,  12,  20, 255)
#define C_HEADER   IM_COL32(  8,  12,  20, 255)
#define C_SEP      IM_COL32( 28,  42,  66, 255)
#define C_ACCENT   IM_COL32( 14, 205, 248, 255)
#define C_ACCENT_G IM_COL32( 14, 205, 248,  35)
#define C_ITEM_HOV IM_COL32( 20,  30,  50, 200)
#define C_ITEM_ACT IM_COL32( 14, 205, 248,  22)
#define C_TEXT     IM_COL32(230, 242, 255, 255)
#define C_DIM      IM_COL32(100, 125, 162, 255)
#define C_CARD     IM_COL32( 16,  22,  36, 255)

static const ImVec4 V_ACCENT = {0.055f, 0.804f, 0.973f, 1.0f};
static const ImVec4 V_TEXT   = {0.902f, 0.949f, 1.000f, 1.0f};
static const ImVec4 V_DIM    = {0.392f, 0.490f, 0.635f, 1.0f};
static const ImVec4 V_GREEN  = {0.350f, 0.950f, 0.550f, 1.0f};
static const ImVec4 V_GOLD   = {1.000f, 0.840f, 0.000f, 1.0f};
static const ImVec4 V_RED    = {0.950f, 0.280f, 0.280f, 1.0f};

// ── Theme ────────────────────────────────────────────────────────────────
void ApplyTheme(int t) {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = s.ChildRounding = s.PopupRounding = 10.0f;
    s.FrameRounding  = s.GrabRounding  = s.TabRounding   =  6.0f;
    s.ScrollbarRounding = 6.0f;
    s.WindowBorderSize = s.FrameBorderSize = s.PopupBorderSize = 0.0f;
    s.WindowPadding    = {20.0f, 16.0f};
    s.FramePadding     = {12.0f,  8.0f};
    s.ItemSpacing      = {12.0f, 10.0f};
    s.ItemInnerSpacing = { 8.0f,  6.0f};
    s.ScrollbarSize    = 5.0f;
    s.GrabMinSize      = 14.0f;

    ImVec4* c = s.Colors;
    if (t == 1) {
        // Cyber Neon
        c[ImGuiCol_WindowBg]          = {0.07f,0.05f,0.12f,0.98f};
        c[ImGuiCol_ChildBg]           = {0.09f,0.06f,0.15f,0.80f};
        c[ImGuiCol_FrameBg]           = {0.16f,0.10f,0.26f,0.75f};
        c[ImGuiCol_FrameBgHovered]    = {0.26f,0.15f,0.40f,0.85f};
        c[ImGuiCol_FrameBgActive]     = {0.36f,0.20f,0.52f,0.95f};
        c[ImGuiCol_CheckMark]         = {0.96f,0.22f,0.76f,1.00f};
        c[ImGuiCol_SliderGrab]        = {0.88f,0.20f,0.68f,1.00f};
        c[ImGuiCol_SliderGrabActive]  = {1.00f,0.35f,0.85f,1.00f};
        c[ImGuiCol_Button]            = {0.28f,0.12f,0.44f,0.85f};
        c[ImGuiCol_ButtonHovered]     = {0.42f,0.18f,0.64f,1.00f};
        c[ImGuiCol_ButtonActive]      = {0.56f,0.25f,0.82f,1.00f};
        c[ImGuiCol_Header]            = {0.32f,0.15f,0.48f,0.80f};
        c[ImGuiCol_HeaderHovered]     = {0.45f,0.22f,0.68f,0.90f};
        c[ImGuiCol_HeaderActive]      = {0.58f,0.28f,0.84f,1.00f};
        c[ImGuiCol_Text]              = {0.96f,0.94f,0.98f,1.00f};
        c[ImGuiCol_TextDisabled]      = {0.55f,0.48f,0.62f,1.00f};
        c[ImGuiCol_Separator]         = {0.32f,0.15f,0.48f,0.50f};
        c[ImGuiCol_ScrollbarBg]       = {0.00f,0.00f,0.00f,0.00f};
        c[ImGuiCol_ScrollbarGrab]     = {0.48f,0.16f,0.72f,0.55f};
        c[ImGuiCol_ScrollbarGrabHovered]={0.65f,0.22f,0.85f,0.80f};
        c[ImGuiCol_ScrollbarGrabActive]= {0.85f,0.25f,0.95f,1.00f};
    } else if (t == 2) {
        // Stealth Dark
        ImGui::StyleColorsDark();
        c[ImGuiCol_WindowBg]          = {0.10f,0.11f,0.14f,0.98f};
        c[ImGuiCol_ChildBg]           = {0.12f,0.13f,0.17f,0.80f};
        c[ImGuiCol_FrameBg]           = {0.15f,0.16f,0.20f,0.80f};
        c[ImGuiCol_CheckMark]         = {0.40f,0.65f,0.95f,1.00f};
        c[ImGuiCol_SliderGrab]        = {0.40f,0.65f,0.95f,1.00f};
        c[ImGuiCol_Button]            = {0.20f,0.22f,0.28f,0.85f};
        c[ImGuiCol_ButtonHovered]     = {0.28f,0.30f,0.38f,1.00f};
        c[ImGuiCol_ButtonActive]      = {0.35f,0.38f,0.46f,1.00f};
        c[ImGuiCol_Separator]         = {0.22f,0.25f,0.32f,0.50f};
        c[ImGuiCol_ScrollbarBg]       = {0.00f,0.00f,0.00f,0.00f};
        c[ImGuiCol_ScrollbarGrab]     = {0.28f,0.32f,0.40f,0.55f};
    } else {
        // Midnight Aurora (default)
        c[ImGuiCol_WindowBg]          = {0.047f,0.063f,0.102f,0.98f};
        c[ImGuiCol_ChildBg]           = {0.063f,0.086f,0.137f,0.80f};
        c[ImGuiCol_FrameBg]           = {0.102f,0.149f,0.239f,0.80f};
        c[ImGuiCol_FrameBgHovered]    = {0.149f,0.220f,0.349f,0.90f};
        c[ImGuiCol_FrameBgActive]     = {0.200f,0.298f,0.459f,1.00f};
        c[ImGuiCol_CheckMark]         = {0.055f,0.804f,0.973f,1.00f};
        c[ImGuiCol_SliderGrab]        = {0.055f,0.710f,0.902f,1.00f};
        c[ImGuiCol_SliderGrabActive]  = {0.120f,0.878f,1.000f,1.00f};
        c[ImGuiCol_Button]            = {0.118f,0.239f,0.380f,0.85f};
        c[ImGuiCol_ButtonHovered]     = {0.176f,0.318f,0.502f,1.00f};
        c[ImGuiCol_ButtonActive]      = {0.239f,0.420f,0.639f,1.00f};
        c[ImGuiCol_Header]            = {0.137f,0.251f,0.400f,0.80f};
        c[ImGuiCol_HeaderHovered]     = {0.196f,0.337f,0.518f,0.90f};
        c[ImGuiCol_HeaderActive]      = {0.259f,0.439f,0.659f,1.00f};
        c[ImGuiCol_Text]              = {0.902f,0.949f,1.000f,1.00f};
        c[ImGuiCol_TextDisabled]      = {0.392f,0.490f,0.635f,1.00f};
        c[ImGuiCol_Separator]         = {0.157f,0.235f,0.357f,0.50f};
        c[ImGuiCol_ScrollbarBg]       = {0.000f,0.000f,0.000f,0.00f};
        c[ImGuiCol_ScrollbarGrab]     = {0.137f,0.447f,0.647f,0.50f};
        c[ImGuiCol_ScrollbarGrabHovered]={0.196f,0.647f,0.851f,0.75f};
        c[ImGuiCol_ScrollbarGrabActive]= {0.247f,0.800f,1.000f,1.00f};
        c[ImGuiCol_PopupBg]           = {0.063f,0.086f,0.137f,0.98f};
    }
}

void Init() {
    s_iconDone = false;
    s_iconTex  = 0;
    ApplyTheme(g_Config.currentTheme);
}

void Shutdown() {
    if (s_iconTex) { glDeleteTextures(1, &s_iconTex); s_iconTex = 0; }
}

bool WantClose() { return g_Config.wantClose; }

// ── Draw helpers ──────────────────────────────────────────────────────────
static void SectionHeader(const char* label) {
    ImGui::Spacing();
    ImGui::TextColored(V_ACCENT, "%s", label);
    ImGui::Separator();
    ImGui::Spacing();
}

static void InfoRow(const char* key, const char* val, ImVec4 vc = {0.902f,0.949f,1.0f,1.0f}) {
    ImGui::TextColored(V_DIM, "%-18s", key);
    ImGui::SameLine(170.0f);
    ImGui::TextColored(vc, "%s", val);
    ImGui::Spacing();
}

// ── Draw ──────────────────────────────────────────────────────────────────
void Draw() {
    LoadIcon();

    ImGuiIO& io   = ImGui::GetIO();
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;

    // Full-screen host window — no decorations, no interaction except children
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({W, H}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##BX", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();

    // ── Dimensions ────────────────────────────────────────────────────────
    const float HDR  = 46.0f;   // header height
    const float SIDE = 120.0f;  // sidebar width

    // ── Background ────────────────────────────────────────────────────────
    dl->AddRectFilled({0,0}, {W,H}, C_BG);

    // ── Header bar ────────────────────────────────────────────────────────
    dl->AddRectFilled({0,0}, {W, HDR}, C_HEADER);
    dl->AddLine({0, HDR}, {W, HDR}, C_SEP, 1.0f);
    dl->AddLine({0, 0},   {W,  0},  C_ACCENT, 2.0f);  // cyan top accent line

    // Logo / title in header
    if (s_iconTex) {
        float iSz = 28.0f;
        float iY  = (HDR - iSz) * 0.5f;
        dl->AddImageRounded((ImTextureID)(intptr_t)s_iconTex,
            {16, iY}, {16+iSz, iY+iSz},
            {0,0}, {1,1}, IM_COL32_WHITE, 6.0f);
    }
    float titleX = s_iconTex ? 52.0f : 16.0f;
    ImGui::SetCursorPos({titleX, (HDR - ImGui::GetTextLineHeight()) * 0.5f - 1.0f});
    ImGui::TextColored(V_ACCENT, "BENZ EX");
    ImGui::SameLine();
    ImGui::TextColored(V_DIM, "Internal ModMenu");

    // Version badge
    const char* ver = "v1.2.0-PRO";
    ImVec2 verSz = ImGui::CalcTextSize(ver);
    ImGui::SetCursorPos({W - verSz.x - 56.0f, (HDR - ImGui::GetTextLineHeight()) * 0.5f});
    ImGui::TextColored(V_DIM, "%s", ver);

    // Close button
    const float cbSz = 30.0f;
    ImGui::SetCursorPos({W - cbSz - 10.0f, (HDR - cbSz) * 0.5f});
    ImGui::PushStyleColor(ImGuiCol_Button,        {0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.85f,0.15f,0.15f,0.30f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.90f,0.10f,0.10f,0.50f});
    ImGui::PushStyleColor(ImGuiCol_Text,          V_RED);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    if (ImGui::Button(ICON_FA_XMARK "##close", {cbSz, cbSz}))
        g_Config.wantClose = true;
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // ── Sidebar ───────────────────────────────────────────────────────────
    dl->AddRectFilled({0, HDR}, {SIDE, H}, C_SIDE);
    dl->AddLine({SIDE, HDR}, {SIDE, H}, C_SEP, 1.0f);

    struct NavItem { const char* icon; const char* label; };
    static const NavItem items[] = {
        { ICON_FA_EYE,         "Visuals"  },
        { ICON_FA_CROSSHAIRS,  "Combat"   },
        { ICON_FA_PALETTE,     "Themes"   },
        { ICON_FA_CIRCLE_INFO, "Info"     },
    };
    static const int NAV_COUNT = 4;
    static int s_tab = 0;

    const float ITEM_H = 56.0f;
    const float ITEM_GAP = 4.0f;
    float navY = HDR + 10.0f;

    for (int i = 0; i < NAV_COUNT; ++i) {
        ImVec2 iMin{0, navY};
        ImVec2 iMax{SIDE, navY + ITEM_H};
        bool hov = ImGui::IsMouseHoveringRect(iMin, iMax);
        bool act = (s_tab == i);

        if (act)       dl->AddRectFilled(iMin, iMax, C_ITEM_ACT);
        else if (hov)  dl->AddRectFilled(iMin, iMax, C_ITEM_HOV);

        // Cyan left bar when active
        if (act) dl->AddRectFilled({0, navY + 4}, {3, navY + ITEM_H - 4}, C_ACCENT);

        // Icon
        ImVec2 iconSz = ImGui::CalcTextSize(items[i].icon);
        ImGui::SetCursorScreenPos({(SIDE - iconSz.x) * 0.5f, navY + 8.0f});
        ImGui::TextColored(act ? V_ACCENT : (hov ? V_TEXT : V_DIM), "%s", items[i].icon);

        // Label
        ImVec2 lblSz = ImGui::CalcTextSize(items[i].label);
        ImGui::SetCursorScreenPos({(SIDE - lblSz.x) * 0.5f, navY + 30.0f});
        ImGui::TextColored(act ? V_TEXT : V_DIM, "%s", items[i].label);

        if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            s_tab = i;

        navY += ITEM_H + ITEM_GAP;
    }

    // ── Content area ──────────────────────────────────────────────────────
    const float cX = SIDE + 1.0f;
    const float cY = HDR  + 1.0f;
    const float cW = W - cX;
    const float cH = H - cY;

    ImGui::SetCursorScreenPos({cX, cY});
    if (ImGui::BeginChild("##content", {cW, cH}, false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

        ImGui::SetCursorPos({24.0f, 18.0f});

        // ── Visuals ───────────────────────────────────────────────────────
        if (s_tab == 0) {
            SectionHeader("VISUAL OPTIONS");

            float col2 = (cW - 48.0f) * 0.50f;

            ImGui::Checkbox("ESP Box",       &g_Config.espBox);
            ImGui::SameLine(col2);
            ImGui::Checkbox("Name & State",  &g_Config.espName);

            ImGui::Spacing();
            ImGui::Checkbox("Snap Lines",    &g_Config.espLines);
            ImGui::SameLine(col2);
            ImGui::Checkbox("Distance Tag",  &g_Config.espDistance);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(V_DIM, "TARGETS");
            ImGui::Spacing();

            ImGui::Checkbox("Granny",    &g_Config.espGranny);
            ImGui::SameLine(col2);
            ImGui::Checkbox("Slendrina", &g_Config.espSlendrina);

            ImGui::Spacing();
            ImGui::Checkbox("Spider",    &g_Config.espSpider);
            ImGui::SameLine(col2);
            ImGui::Checkbox("Rat",       &g_Config.espRat);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(V_DIM, "RANGE");
            ImGui::Spacing();
            ImGui::SetNextItemWidth(cW - 68.0f);
            ImGui::SliderFloat("##dist", &g_Config.espMaxDistance, 5.0f, 150.0f,
                "Max Distance: %.0f m");
        }

        // ── Combat ────────────────────────────────────────────────────────
        else if (s_tab == 1) {
            SectionHeader("COMBAT OPTIONS");
            ImGui::TextColored(V_DIM, "Connect game hooks to enable combat features.");
        }

        // ── Themes ────────────────────────────────────────────────────────
        else if (s_tab == 2) {
            SectionHeader("COLOR SCHEMES");

            const char* themes[] = {"Midnight Aurora", "Cyber Neon", "Stealth Dark"};
            ImGui::SetNextItemWidth(260.0f);
            if (ImGui::Combo("##theme", &g_Config.currentTheme, themes, 3))
                ApplyTheme(g_Config.currentTheme);

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {16.0f, 10.0f});
            if (ImGui::Button("Reset to Defaults", {200.0f, 0.0f})) {
                g_Config = Config();
                ApplyTheme(g_Config.currentTheme);
            }
            ImGui::PopStyleVar();
        }

        // ── Info ──────────────────────────────────────────────────────────
        else if (s_tab == 3) {
            SectionHeader("BUILD INFORMATION");

            InfoRow("Developer",   "BENZ EX",              V_GOLD);
            InfoRow("Project",     "Internal ModMenu");
            InfoRow("Version",     "v1.2.0-PRO",           V_GREEN);
            InfoRow("Platform",    "Windows x64");
            InfoRow("Renderer",    "OpenGL 3.3 Core");
            InfoRow("Backend",     "GLFW + ImGui");

            char fps[48];
            snprintf(fps, sizeof(fps), "%.1f FPS  |  %dx%d",
                io.Framerate, (int)W, (int)H);
            InfoRow("Performance", fps, V_ACCENT);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(V_DIM, "Internal ModMenu — Developed by BENZ EX");
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace MenuUI
