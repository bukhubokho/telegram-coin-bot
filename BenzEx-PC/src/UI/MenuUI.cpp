#include "MenuUI.h"
#include "imgui_internal.h"
#include "Includes/FontAwesomeIcons.h"
#include "Includes/MenuIconData.h"
#include "stb_image.h"
#include <glad/glad.h>   // desktop GL -- or use: #include <GL/gl.h>
#include <cmath>
#include <algorithm>
#include <atomic>

namespace MenuUI {

Config g_Config;

static std::atomic<int> s_boundX{80};
static std::atomic<int> s_boundY{60};
static std::atomic<int> s_boundW{720};
static std::atomic<int> s_boundH{460};
static bool s_needCenter = true;

static GLuint s_iconTexture = 0;
static bool s_iconLoaded = false;

static void EnsureIconTexture() {
    if (s_iconLoaded) return;
    s_iconLoaded = true;

    int w = 0, h = 0, comp = 0;
    unsigned char* pixels = stbi_load_from_memory(menu_icon_png, (int)menu_icon_png_len, &w, &h, &comp, 4);
    if (!pixels) return;

    glGenTextures(1, &s_iconTexture);
    glBindTexture(GL_TEXTURE_2D, s_iconTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);
}

// Senior smooth drawing helpers: 64-segment circle and 48-segment rounded rectangle
static void AddImageCircle(ImDrawList* dl, ImTextureID user_texture_id, const ImVec2& center, float radius, ImU32 col = IM_COL32_WHITE, int num_segments = 64) {
    if ((col & IM_COL32_A_MASK) == 0 || user_texture_id == 0) return;

    dl->PushTextureID(user_texture_id);
    int vert_start_idx = dl->VtxBuffer.Size;

    dl->PathArcTo(center, radius, 0.0f, IM_PI * 2.0f, num_segments);
    dl->PathFillConvex(col);

    int vert_end_idx = dl->VtxBuffer.Size;
    ImVec2 p_min(center.x - radius, center.y - radius);
    ImVec2 p_max(center.x + radius, center.y + radius);
    ImGui::ShadeVertsLinearUV(dl, vert_start_idx, vert_end_idx, p_min, p_max, ImVec2(0, 0), ImVec2(1, 1), true);

    dl->PopTextureID();
}

static void AddImageRoundedSmooth(ImDrawList* dl, ImTextureID user_texture_id, const ImVec2& p_min, const ImVec2& p_max, float rounding, ImU32 col = IM_COL32_WHITE, int corner_segments = 12) {
    if ((col & IM_COL32_A_MASK) == 0 || user_texture_id == 0) return;

    dl->PushTextureID(user_texture_id);
    int vert_start_idx = dl->VtxBuffer.Size;

    float r = std::min(rounding, std::min((p_max.x - p_min.x) * 0.5f, (p_max.y - p_min.y) * 0.5f));
    dl->PathArcTo(ImVec2(p_max.x - r, p_min.y + r), r, -IM_PI * 0.5f, 0.0f, corner_segments);
    dl->PathArcTo(ImVec2(p_max.x - r, p_max.y - r), r, 0.0f, IM_PI * 0.5f, corner_segments);
    dl->PathArcTo(ImVec2(p_min.x + r, p_max.y - r), r, IM_PI * 0.5f, IM_PI, corner_segments);
    dl->PathArcTo(ImVec2(p_min.x + r, p_min.y + r), r, IM_PI, IM_PI * 1.5f, corner_segments);

    dl->PathFillConvex(col);

    int vert_end_idx = dl->VtxBuffer.Size;

    // Aspect-ratio preserving UV mapping (keep square texture 1:1 centered inside box)
    float boxW = p_max.x - p_min.x;
    float boxH = p_max.y - p_min.y;
    float sqSide = std::min(boxW, boxH);
    float cx = (p_min.x + p_max.x) * 0.5f;
    float cy = (p_min.y + p_max.y) * 0.5f;
    ImVec2 sqMin(cx - sqSide * 0.5f, cy - sqSide * 0.5f);
    ImVec2 sqMax(cx + sqSide * 0.5f, cy + sqSide * 0.5f);

    ImGui::ShadeVertsLinearUV(dl, vert_start_idx, vert_end_idx, sqMin, sqMax, ImVec2(0, 0), ImVec2(1, 1), true);

    dl->PopTextureID();
}

static void AddRectSmooth(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, float thickness = 1.0f, int corner_segments = 12) {
    float r = std::min(rounding, std::min((p_max.x - p_min.x) * 0.5f, (p_max.y - p_min.y) * 0.5f));
    dl->PathArcTo(ImVec2(p_max.x - r, p_min.y + r), r, -IM_PI * 0.5f, 0.0f, corner_segments);
    dl->PathArcTo(ImVec2(p_max.x - r, p_max.y - r), r, 0.0f, IM_PI * 0.5f, corner_segments);
    dl->PathArcTo(ImVec2(p_min.x + r, p_max.y - r), r, IM_PI * 0.5f, IM_PI, corner_segments);
    dl->PathArcTo(ImVec2(p_min.x + r, p_min.y + r), r, IM_PI, IM_PI * 1.5f, corner_segments);
    dl->PathStroke(col, ImDrawFlags_Closed, thickness);
}

static void AddRectFilledSmooth(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, int corner_segments = 12) {
    float r = std::min(rounding, std::min((p_max.x - p_min.x) * 0.5f, (p_max.y - p_min.y) * 0.5f));
    dl->PathArcTo(ImVec2(p_max.x - r, p_min.y + r), r, -IM_PI * 0.5f, 0.0f, corner_segments);
    dl->PathArcTo(ImVec2(p_max.x - r, p_max.y - r), r, 0.0f, IM_PI * 0.5f, corner_segments);
    dl->PathArcTo(ImVec2(p_min.x + r, p_max.y - r), r, IM_PI * 0.5f, IM_PI, corner_segments);
    dl->PathArcTo(ImVec2(p_min.x + r, p_min.y + r), r, IM_PI, IM_PI * 1.5f, corner_segments);
    dl->PathFillConvex(col);
}

void ApplyTheme(int themeIndex) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding    = 14.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;

    style.WindowBorderSize  = 0.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 0.0f;

    style.WindowPadding     = ImVec2(20.0f, 16.0f);
    style.FramePadding      = ImVec2(12.0f, 8.0f);
    style.ItemSpacing       = ImVec2(14.0f, 12.0f);
    style.ItemInnerSpacing  = ImVec2(10.0f, 8.0f);
    style.TouchExtraPadding = ImVec2(4.0f, 4.0f);
    style.ScrollbarSize     = 6.0f;
    style.GrabMinSize       = 20.0f;

    if (themeIndex == 1) {
        // Cyber Neon (Magenta / Electric Purple)
        colors[ImGuiCol_WindowBg]             = ImVec4(0.07f, 0.05f, 0.12f, 0.96f);
        colors[ImGuiCol_Border]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TitleBg]              = ImVec4(0.14f, 0.08f, 0.22f, 1.00f);
        colors[ImGuiCol_TitleBgActive]        = ImVec4(0.24f, 0.10f, 0.38f, 1.00f);
        colors[ImGuiCol_FrameBg]              = ImVec4(0.16f, 0.10f, 0.26f, 0.75f);
        colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.26f, 0.15f, 0.40f, 0.85f);
        colors[ImGuiCol_FrameBgActive]        = ImVec4(0.36f, 0.20f, 0.52f, 0.95f);
        colors[ImGuiCol_CheckMark]            = ImVec4(0.96f, 0.22f, 0.76f, 1.00f);
        colors[ImGuiCol_SliderGrab]           = ImVec4(0.88f, 0.20f, 0.68f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]     = ImVec4(1.00f, 0.35f, 0.85f, 1.00f);
        colors[ImGuiCol_Button]               = ImVec4(0.28f, 0.12f, 0.44f, 0.85f);
        colors[ImGuiCol_ButtonHovered]        = ImVec4(0.42f, 0.18f, 0.64f, 1.00f);
        colors[ImGuiCol_ButtonActive]         = ImVec4(0.56f, 0.25f, 0.82f, 1.00f);
        colors[ImGuiCol_Header]               = ImVec4(0.32f, 0.15f, 0.48f, 0.80f);
        colors[ImGuiCol_HeaderHovered]        = ImVec4(0.45f, 0.22f, 0.68f, 0.90f);
        colors[ImGuiCol_HeaderActive]         = ImVec4(0.58f, 0.28f, 0.84f, 1.00f);
        colors[ImGuiCol_Tab]                  = ImVec4(0.18f, 0.10f, 0.28f, 0.85f);
        colors[ImGuiCol_TabHovered]           = ImVec4(0.38f, 0.18f, 0.58f, 0.90f);
        colors[ImGuiCol_TabActive]            = ImVec4(0.48f, 0.20f, 0.72f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.48f, 0.16f, 0.72f, 0.60f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.65f, 0.22f, 0.85f, 0.85f);
        colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.85f, 0.25f, 0.95f, 1.00f);
        colors[ImGuiCol_Text]                 = ImVec4(0.96f, 0.94f, 0.98f, 1.00f);
        colors[ImGuiCol_TextDisabled]         = ImVec4(0.55f, 0.48f, 0.62f, 1.00f);
        colors[ImGuiCol_Separator]            = ImVec4(0.32f, 0.15f, 0.48f, 0.60f);
    } else if (themeIndex == 2) {
        // Stealth Dark (Monochrome / Slate)
        ImGui::StyleColorsDark();
        colors[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.11f, 0.14f, 0.96f);
        colors[ImGuiCol_Border]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TitleBg]              = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
        colors[ImGuiCol_TitleBgActive]        = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
        colors[ImGuiCol_FrameBg]              = ImVec4(0.15f, 0.16f, 0.20f, 0.80f);
        colors[ImGuiCol_Button]               = ImVec4(0.20f, 0.22f, 0.28f, 0.85f);
        colors[ImGuiCol_ButtonHovered]        = ImVec4(0.28f, 0.30f, 0.38f, 1.00f);
        colors[ImGuiCol_ButtonActive]         = ImVec4(0.35f, 0.38f, 0.46f, 1.00f);
        colors[ImGuiCol_CheckMark]            = ImVec4(0.40f, 0.65f, 0.95f, 1.00f);
        colors[ImGuiCol_SliderGrab]           = ImVec4(0.40f, 0.65f, 0.95f, 1.00f);
        colors[ImGuiCol_Tab]                  = ImVec4(0.14f, 0.15f, 0.18f, 0.85f);
        colors[ImGuiCol_TabHovered]           = ImVec4(0.24f, 0.26f, 0.32f, 0.90f);
        colors[ImGuiCol_TabActive]            = ImVec4(0.30f, 0.34f, 0.42f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.32f, 0.40f, 0.60f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.42f, 0.52f, 0.85f);
        colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.48f, 0.55f, 0.68f, 1.00f);
        colors[ImGuiCol_Separator]            = ImVec4(0.22f, 0.25f, 0.32f, 0.60f);
    } else {
        // Midnight Aurora (Deep navy + cyan accent - default)
        colors[ImGuiCol_WindowBg]             = ImVec4(0.06f, 0.08f, 0.13f, 0.96f);
        colors[ImGuiCol_Border]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TitleBg]              = ImVec4(0.08f, 0.12f, 0.19f, 1.00f);
        colors[ImGuiCol_TitleBgActive]        = ImVec4(0.11f, 0.18f, 0.28f, 1.00f);
        colors[ImGuiCol_FrameBg]              = ImVec4(0.10f, 0.15f, 0.24f, 0.80f);
        colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.15f, 0.24f, 0.36f, 0.90f);
        colors[ImGuiCol_FrameBgActive]        = ImVec4(0.20f, 0.30f, 0.46f, 1.00f);
        colors[ImGuiCol_CheckMark]            = ImVec4(0.12f, 0.80f, 0.95f, 1.00f);
        colors[ImGuiCol_SliderGrab]           = ImVec4(0.12f, 0.72f, 0.90f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.22f, 0.88f, 1.00f, 1.00f);
        colors[ImGuiCol_Button]               = ImVec4(0.12f, 0.24f, 0.38f, 0.85f);
        colors[ImGuiCol_ButtonHovered]        = ImVec4(0.18f, 0.32f, 0.50f, 1.00f);
        colors[ImGuiCol_ButtonActive]         = ImVec4(0.24f, 0.42f, 0.64f, 1.00f);
        colors[ImGuiCol_Header]               = ImVec4(0.14f, 0.25f, 0.40f, 0.80f);
        colors[ImGuiCol_HeaderHovered]        = ImVec4(0.20f, 0.34f, 0.52f, 0.90f);
        colors[ImGuiCol_HeaderActive]         = ImVec4(0.26f, 0.44f, 0.66f, 1.00f);
        colors[ImGuiCol_Tab]                  = ImVec4(0.09f, 0.14f, 0.22f, 0.85f);
        colors[ImGuiCol_TabHovered]           = ImVec4(0.16f, 0.26f, 0.40f, 0.90f);
        colors[ImGuiCol_TabActive]            = ImVec4(0.20f, 0.34f, 0.52f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.14f, 0.45f, 0.65f, 0.55f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.20f, 0.65f, 0.85f, 0.80f);
        colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.25f, 0.80f, 1.00f, 1.00f);
        colors[ImGuiCol_Text]                 = ImVec4(0.92f, 0.96f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]         = ImVec4(0.46f, 0.55f, 0.68f, 1.00f);
        colors[ImGuiCol_Separator]            = ImVec4(0.16f, 0.24f, 0.36f, 0.60f);
    }
}

void Init() {
    s_iconLoaded = false;
    s_iconTexture = 0;
    ApplyTheme(g_Config.currentTheme);
    s_needCenter = true;
}

void Shutdown() {
    if (s_iconTexture != 0) {
        glDeleteTextures(1, &s_iconTexture);
        s_iconTexture = 0;
    }
    s_iconLoaded = false;
}

void GetBounds(int& x, int& y, int& w, int& h) {
    x = s_boundX.load(std::memory_order_relaxed);
    y = s_boundY.load(std::memory_order_relaxed);
    w = s_boundW.load(std::memory_order_relaxed);
    h = s_boundH.load(std::memory_order_relaxed);
}

static void DrawFloatingBubble() {
    EnsureIconTexture();

    const float bubbleWindowSize = 120.0f;
    const float radius = 50.0f; // Large, smooth 100px round floating bubble

    ImGui::SetNextWindowSize(ImVec2(bubbleWindowSize, bubbleWindowSize), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##FloatingBubble", nullptr, flags)) {
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 center(pos.x + bubbleWindowSize * 0.5f, pos.y + bubbleWindowSize * 0.5f);

        s_boundX.store((int)(center.x - radius), std::memory_order_relaxed);
        s_boundY.store((int)(center.y - radius), std::memory_order_relaxed);
        s_boundW.store((int)(radius * 2.0f), std::memory_order_relaxed);
        s_boundH.store((int)(radius * 2.0f), std::memory_order_relaxed);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Breathing pulse animation effect
        static float animTime = 0.0f;
        animTime += ImGui::GetIO().DeltaTime;
        float pulse = 0.5f + 0.5f * std::sin(animTime * 2.5f);

        // 1. Soft breathing outer glow aura (circular 64 segments)
        dl->AddCircle(center, radius + 4.0f, IM_COL32(32, 210, 245, (int)(55 + pulse * 50)), 64, 3.0f);

        // 2. Crisp circular white background (64 segments)
        dl->AddCircleFilled(center, radius, IM_COL32(255, 255, 255, 255), 64);

        // 3. Custom image icon rendered as a perfect 64-segment circle (zero clipping, 100% complete emblem)
        if (s_iconTexture != 0) {
            AddImageCircle(dl, (ImTextureID)(intptr_t)s_iconTexture, center, radius, IM_COL32_WHITE, 64);
        } else {
            ImVec2 gSize = ImGui::CalcTextSize(ICON_FA_GAMEPAD);
            dl->AddText(ImVec2(center.x - gSize.x * 0.5f, center.y - gSize.y * 0.5f),
                        IM_COL32(35, 215, 245, 255), ICON_FA_GAMEPAD);
        }

        // 4. Deep dark under-ring for sharp contrast and depth (64 segments)
        dl->AddCircle(center, radius, IM_COL32(5, 20, 45, 255), 64, 4.0f);

        // 5. Bold saturated cyan neon border ring (64 segments)
        dl->AddCircle(center, radius, IM_COL32(14, 205, 248, 255), 64, 2.8f);

        // Dragging and click-to-expand handling
        float hitOffset = (bubbleWindowSize - radius * 2.0f) * 0.5f;
        ImGui::SetCursorPos(ImVec2(hitOffset, hitOffset));
        ImGui::InvisibleButton("##bubbleHit", ImVec2(radius * 2.0f, radius * 2.0f));

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            ImVec2 dragDelta = ImGui::GetMouseDragDelta(0);
            ImGui::SetWindowPos("##FloatingBubble", ImVec2(pos.x + dragDelta.x, pos.y + dragDelta.y));
            ImGui::ResetMouseDragDelta(0);
        } else if (ImGui::IsItemDeactivated() && !ImGui::IsMouseDragPastThreshold(0)) {
            g_Config.isMinimized = false;
            s_needCenter = true; // Center menu when reopening
        }
    }
    ImGui::End();
}

void Draw() {
    EnsureIconTexture();

    if (g_Config.isMinimized) {
        DrawFloatingBubble();
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;
    bool isLandscape = (screenW >= screenH);

    // Expanded 2x scale for comfortable readability and layout
    float menuW = isLandscape ? std::clamp(screenW * 0.90f, 400.0f, 1650.0f)
                              : std::clamp(screenW * 0.92f, 360.0f, 1200.0f);
    float menuH = isLandscape ? std::clamp(screenH * 0.88f, 340.0f, 1050.0f)
                              : std::clamp(screenH * 0.88f, 400.0f, 1400.0f);

    if (s_needCenter) {
        ImVec2 centerPos((screenW - menuW) * 0.5f, (screenH - menuH) * 0.5f);
        ImGui::SetNextWindowPos(centerPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(menuW, menuH), ImGuiCond_Always);
        s_needCenter = false;
    } else {
        ImGui::SetNextWindowSize(ImVec2(menuW, menuH), ImGuiCond_FirstUseEver);
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin("Internal ModMenu", nullptr, flags)) {
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        s_boundX.store((int)pos.x, std::memory_order_relaxed);
        s_boundY.store((int)pos.y, std::memory_order_relaxed);
        s_boundW.store((int)size.x, std::memory_order_relaxed);
        s_boundH.store((int)size.y, std::memory_order_relaxed);

        // ── Top Header Bar ──────────────────────────────────────────
        const float headerTopY = 12.0f;
        const float headerIconSize = 44.0f;
        const float headerIconRounding = 10.0f;

        ImDrawList* wDl = ImGui::GetWindowDrawList();

        // 1. Header Icon (crisp, uncropped emblem with double-layer border)
        ImVec2 iconTopLeft(pos.x + 18.0f, pos.y + headerTopY);
        ImVec2 iconBottomRight(pos.x + 18.0f + headerIconSize, pos.y + headerTopY + headerIconSize);

        if (s_iconTexture != 0) {
            AddRectFilledSmooth(wDl, iconTopLeft, iconBottomRight, IM_COL32(255, 255, 255, 255), headerIconRounding, 12);
            AddImageRoundedSmooth(wDl, (ImTextureID)(intptr_t)s_iconTexture, iconTopLeft, iconBottomRight, headerIconRounding, IM_COL32_WHITE, 12);
            // Deep dark under-stroke and bold saturated cyan stroke (smooth 48-segment contour)
            AddRectSmooth(wDl, iconTopLeft, iconBottomRight, IM_COL32(5, 20, 45, 255), headerIconRounding, 3.6f, 12);
            AddRectSmooth(wDl, iconTopLeft, iconBottomRight, IM_COL32(14, 205, 248, 255), headerIconRounding, 2.4f, 12);
            ImGui::SetCursorPos(ImVec2(72.0f, headerTopY + 5.0f));
        } else {
            ImGui::SetCursorPos(ImVec2(20.0f, headerTopY + 5.0f));
        }

        // 2. Title & Author Tag (using ASCII '|' to guarantee 100% font compatibility)
        ImGui::TextColored(ImVec4(0.18f, 0.88f, 1.00f, 1.00f), "Internal ModMenu");
        ImGui::SameLine();
        ImGui::TextDisabled("| BENZ EX");

        // 3. Top-Right Power Off Button
        // Clean alignment: zero frame padding to prevent glyph clipping, 16px right margin
        const float btnW = 46.0f;
        const float btnH = 38.0f;
        float btnX = size.x - btnW - 16.0f;
        float btnY = headerTopY + (headerIconSize - btnH) * 0.5f;

        ImGui::SetCursorPos(ImVec2(btnX, btnY));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 0.20f, 0.20f, 0.18f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.00f, 0.10f, 0.15f, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.25f, 0.28f, 1.00f));
        if (ImGui::Button(ICON_FA_POWER_OFF "##closeBtn", ImVec2(btnW, btnH))) {
            g_Config.isMinimized = true;
        }
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();

        ImGui::SetCursorPosY(headerTopY + headerIconSize + 10.0f);
        ImGui::Separator();
        ImGui::Spacing();

        float col2Offset = std::max(size.x * 0.48f, 340.0f);

        if (ImGui::BeginTabBar("##MainTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
            // ── Tab 1: Visuals ──────────────────────────────
            if (ImGui::BeginTabItem(ICON_FA_EYE " Visuals")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.20f, 0.88f, 1.0f, 1.0f), "VISUAL OPTIONS");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Checkbox("ESP Box", &g_Config.espBox);
                ImGui::SameLine(col2Offset);
                ImGui::Checkbox("Name & State", &g_Config.espName);

                ImGui::Spacing();
                ImGui::Checkbox("Snap Lines", &g_Config.espLines);
                ImGui::SameLine(col2Offset);
                ImGui::Checkbox("Distance Tag", &g_Config.espDistance);

                ImGui::Spacing();
                ImGui::TextDisabled("Targets");
                ImGui::Checkbox("Granny", &g_Config.espGranny);
                ImGui::SameLine(col2Offset);
                ImGui::Checkbox("Slendrina", &g_Config.espSlendrina);
                ImGui::Checkbox("Spider", &g_Config.espSpider);
                ImGui::SameLine(col2Offset);
                ImGui::Checkbox("Rat", &g_Config.espRat);

                ImGui::Spacing();
                ImGui::SliderFloat("Max Distance", &g_Config.espMaxDistance, 5.0f, 150.0f, "%.0f m");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextDisabled("UI-only template — connect game features separately.");

                ImGui::EndTabItem();
            }

            // ── Tab 2: Debug ────────────────────────────────
            if (ImGui::BeginTabItem(ICON_FA_CODE " Debug")) {
                ImGui::Spacing();
                ImGui::TextDisabled("BENZEX UI template");
                ImGui::Text("This tab is intentionally free of game-specific logic.");
                ImGui::EndTabItem();
            }

            // ── Tab 3: Themes ───────────────────────────────
            if (ImGui::BeginTabItem(ICON_FA_PALETTE " Themes")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.20f, 0.88f, 1.0f, 1.0f), "COLOR SCHEMES & PREFERENCES");
                ImGui::Separator();
                ImGui::Spacing();

                const char* themes[] = {"Midnight Aurora", "Cyber Neon", "Stealth Dark"};
                if (ImGui::Combo("Select Theme", &g_Config.currentTheme, themes, IM_ARRAYSIZE(themes))) {
                    ApplyTheme(g_Config.currentTheme);
                }

                ImGui::Spacing();
                if (ImGui::Button("Reset to Defaults", ImVec2(0.0f, 48.0f))) {
                    g_Config = Config();
                    ApplyTheme(g_Config.currentTheme);
                    s_needCenter = true;
                }

                ImGui::EndTabItem();
            }

            // ── Tab 4: Developer & Info ─────────────────────
            if (ImGui::BeginTabItem(ICON_FA_USER " Developer")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.20f, 0.88f, 1.0f, 1.0f), "DEVELOPER & BUILD INFORMATION");
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::BeginTable("##DevTable", 2, ImGuiTableFlags_None)) {
                    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    // Developer
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("Developer");
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImVec4(1.00f, 0.84f, 0.00f, 1.0f), "BENZ EX");

                    // Project
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("Project");
                    ImGui::TableNextColumn();
                    ImGui::Text("Internal ModMenu");

                    // Version
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("Version");
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.55f, 1.0f), "v1.2.0-PRO (Full Release)");

                    // Environment
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("Environment");
                    ImGui::TableNextColumn();
                    ImGui::Text("Standalone Floating GLES3");

                    // Architecture
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("Architecture");
                    ImGui::TableNextColumn();
                    ImGui::Text("ARM64-v8a");

                    // Graphics Backend
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("Renderer");
                    ImGui::TableNextColumn();
                    ImGui::Text("OpenGL ES 3.0 (EGL)");

                    // Telemetry
                    ImGuiIO& io = ImGui::GetIO();
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("Performance");
                    ImGui::TableNextColumn();
                    ImGui::Text("%.1f FPS | %.0f x %.0f", io.Framerate, io.DisplaySize.x, io.DisplaySize.y);

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextDisabled("Internal ModMenu | Developed by BENZ EX");

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

} // namespace MenuUI
