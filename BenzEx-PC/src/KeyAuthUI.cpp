// language: C++17, file: KeyAuthUI.cpp, target: Windows 11, MSVC
#include "KeyAuthUI.h"
#include "imgui.h"
#include "imgui_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <intrin.h>

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace KeyAuthUI {

// ── Palette (matches MenuUI Midnight Aurora) ──────────────────────────────
#define COL_ACCENT      IM_COL32( 14, 205, 248, 255)   // cyan
#define COL_ACCENT_DIM  IM_COL32( 14, 205, 248, 110)
#define COL_ACCENT_GLOW IM_COL32( 14, 205, 248,  55)
#define COL_BG          IM_COL32( 15,  20,  33, 252)   // matches WindowBg
#define COL_BG_CARD     IM_COL32( 18,  25,  42, 255)
#define COL_BG_FRAME    IM_COL32( 26,  38,  61, 200)
#define COL_TEXT        IM_COL32(235, 245, 255, 255)
#define COL_TEXT_DIM    IM_COL32(118, 141, 175, 255)
#define COL_BTN         IM_COL32( 30,  61,  97, 215)
#define COL_BTN_HOV     IM_COL32( 45,  82, 128, 255)
#define COL_BTN_ACT     IM_COL32( 61, 107, 163, 255)
#define COL_ERROR       IM_COL32(255,  72,  72, 230)

// ── State ─────────────────────────────────────────────────────────────────
static std::atomic<AuthState> s_state{ AuthState::Idle };
static std::string s_hwid;
static char s_keyBuf[64] = {};
static std::string s_errorMsg;
static std::mutex s_msgMtx;

// ── HWID ──────────────────────────────────────────────────────────────────
static uint64_t FoldBytes(const void* data, size_t len) {
    uint64_t h = 14695981039346656037ULL;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}

std::string HwidGet() {
    if (!s_hwid.empty()) return s_hwid;
    uint64_t seed = 0;
    DWORD serial = 0;
    GetVolumeInformationW(L"C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
    seed ^= FoldBytes(&serial, sizeof(serial));
    int cpuInfo[4] = {};
    __cpuid(cpuInfo, 0); seed ^= FoldBytes(cpuInfo, sizeof(cpuInfo));
    __cpuid(cpuInfo, 1); seed ^= FoldBytes(cpuInfo, sizeof(cpuInfo));
    ULONG bufLen = 15000;
    auto adapters = std::make_unique<uint8_t[]>(bufLen);
    if (GetAdaptersInfo(reinterpret_cast<IP_ADAPTER_INFO*>(adapters.get()), &bufLen) == ERROR_SUCCESS) {
        auto* a = reinterpret_cast<IP_ADAPTER_INFO*>(adapters.get());
        while (a) { if (a->AddressLength == 6) { seed ^= FoldBytes(a->Address, 6); break; } a = a->Next; }
    }
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << seed;
    s_hwid = oss.str();
    return s_hwid;
}

// ── HTTP Activate ─────────────────────────────────────────────────────────
static void DoActivate(std::string key, std::string hwid) {
    auto setErr = [](const std::string& msg) {
        std::lock_guard<std::mutex> lk(s_msgMtx);
        s_errorMsg = msg;
        s_state = AuthState::Failed;
    };
    HINTERNET hSes = WinHttpOpen(L"BenzEx/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSes) { setErr("Network error (session)"); return; }
    HINTERNET hCon = WinHttpConnect(hSes, L"telegram-coin-bot-1.onrender.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hCon) { WinHttpCloseHandle(hSes); setErr("Network error (connect)"); return; }
    HINTERNET hReq = WinHttpOpenRequest(hCon, L"POST", L"/api/activate",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hCon); WinHttpCloseHandle(hSes); setErr("Network error (request)"); return; }
    std::string body = "{\"key\":\"" + key + "\",\"hwid\":\"" + hwid + "\"}";
    BOOL sent = WinHttpSendRequest(hReq, L"Content-Type: application/json\r\n", (DWORD)-1,
        (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent || !WinHttpReceiveResponse(hReq, nullptr)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hCon); WinHttpCloseHandle(hSes);
        setErr("Server unreachable"); return;
    }
    std::string response;
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
        std::string chunk(avail, '\0'); DWORD read = 0;
        WinHttpReadData(hReq, chunk.data(), avail, &read);
        response.append(chunk.data(), read);
    }
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hCon); WinHttpCloseHandle(hSes);
    if (response.find("\"ok\":true") != std::string::npos ||
        response.find("\"ok\": true") != std::string::npos) {
        s_state = AuthState::Success;
    } else {
        std::string msg = "Invalid key";
        auto pos = response.find("\"message\":\"");
        if (pos == std::string::npos) pos = response.find("\"msg\":\"");
        if (pos != std::string::npos) {
            pos = response.find('"', pos + 1);
            pos = response.find('"', pos + 1);
            auto end = response.find('"', pos + 1);
            if (end != std::string::npos) msg = response.substr(pos + 1, end - pos - 1);
        }
        setErr(msg);
    }
}

void Init() { HwidGet(); }

// ── Draw ──────────────────────────────────────────────────────────────────
void Draw() {
    if (s_state == AuthState::Success) return;

    ImGuiIO& io = ImGui::GetIO();

    // Full-window dark overlay
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0, 0), io.DisplaySize, IM_COL32(6, 8, 13, 235));

    const float W = io.DisplaySize.x, H = io.DisplaySize.y;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(W, H));
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGui::Begin("##auth", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Card dimensions - match menu proportions
    const float cW = 460.0f, cH = 310.0f;
    const float cX = (W - cW) * 0.5f, cY = (H - cH) * 0.5f;
    const float r  = 14.0f;

    // Subtle outer glow
    dl->AddRectFilled(ImVec2(cX - 8, cY - 8), ImVec2(cX + cW + 8, cY + cH + 8),
        COL_ACCENT_GLOW, r + 8.0f);

    // Card background
    dl->AddRectFilled(ImVec2(cX, cY), ImVec2(cX + cW, cY + cH),
        COL_BG_CARD, r);

    // Top accent bar (matches menu header separator style)
    dl->AddRectFilled(ImVec2(cX + r, cY), ImVec2(cX + cW - r, cY + 3.0f),
        COL_ACCENT, 2.0f);

    // Card border
    dl->AddRect(ImVec2(cX, cY), ImVec2(cX + cW, cY + cH),
        COL_ACCENT_DIM, r, 0, 1.2f);

    // ── Content (use SetCursorScreenPos for pixel-perfect placement) ──

    // Title — matches "Internal ModMenu" style in menu
    const char* title = "BENZ EX  |  Key Activation";
    ImVec2 tSize = ImGui::CalcTextSize(title);
    ImGui::SetCursorScreenPos(ImVec2(cX + (cW - tSize.x) * 0.5f, cY + 20.0f));
    ImGui::TextColored(ImVec4(0.055f, 0.804f, 0.973f, 1.0f), "%s", title);

    // Thin separator under title
    dl->AddLine(ImVec2(cX + 20, cY + 46), ImVec2(cX + cW - 20, cY + 46),
        COL_ACCENT_DIM, 0.8f);

    // HWID row
    std::string hwidLabel = "HWID: " + s_hwid;
    ImVec2 hSize = ImGui::CalcTextSize(hwidLabel.c_str());
    ImGui::SetCursorScreenPos(ImVec2(cX + (cW - hSize.x) * 0.5f, cY + 56.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.463f, 0.553f, 0.686f, 1.0f));
    ImGui::TextUnformatted(hwidLabel.c_str());
    if (ImGui::IsItemClicked()) ImGui::SetClipboardText(s_hwid.c_str());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to copy");
    ImGui::PopStyleColor();

    // "Enter your key" label
    ImGui::SetCursorScreenPos(ImVec2(cX + 30.0f, cY + 96.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.96f, 1.0f, 1.0f));
    ImGui::Text("Enter your activation key:");
    ImGui::PopStyleColor();

    // Input field — matches FrameBg from ApplyTheme
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.102f, 0.149f, 0.239f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  ImVec4(0.149f, 0.239f, 0.361f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,   ImVec4(0.200f, 0.302f, 0.459f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Border,          ImVec4(0.055f, 0.804f, 0.973f, 0.35f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12.0f, 9.0f));

    bool disabled = (s_state == AuthState::Checking);
    if (disabled) ImGui::BeginDisabled();

    ImGui::SetCursorScreenPos(ImVec2(cX + 30.0f, cY + 122.0f));
    ImGui::SetNextItemWidth(cW - 60.0f);
    bool enter = ImGui::InputText("##key", s_keyBuf, sizeof(s_keyBuf),
        ImGuiInputTextFlags_EnterReturnsTrue);

    if (disabled) ImGui::EndDisabled();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    // Activate button — matches ImGuiCol_Button from ApplyTheme
    const float btnW = 180.0f, btnH = 40.0f;
    const float btnX2 = cX + (cW - btnW) * 0.5f;
    const float btnY2 = cY + 182.0f;

    ImGui::SetCursorScreenPos(ImVec2(btnX2, btnY2));

    if (s_state == AuthState::Checking) {
        static float rot = 0.0f;
        rot += io.DeltaTime * 180.0f;
        const char* frames[] = { "|", "/", "\xe2\x80\x94", "\\" };
        int idx = (int)(rot / 45.0f) % 4;
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.118f, 0.239f, 0.380f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.118f, 0.239f, 0.380f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.055f, 0.804f, 0.973f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::Button(("Checking... " + std::string(frames[idx])).c_str(), ImVec2(btnW, btnH));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.118f, 0.239f, 0.380f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.176f, 0.318f, 0.502f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.239f, 0.420f, 0.639f, 1.00f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        bool clicked = ImGui::Button("  Activate  ", ImVec2(btnW, btnH)) || enter;
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (clicked && s_keyBuf[0] != '\0') {
            s_state = AuthState::Checking;
            s_errorMsg.clear();
            std::string k(s_keyBuf), h = s_hwid;
            std::thread([k, h]() { DoActivate(k, h); }).detach();
        }
    }

    // Error message
    if (s_state == AuthState::Failed) {
        std::lock_guard<std::mutex> lk(s_msgMtx);
        ImVec2 eSize = ImGui::CalcTextSize(s_errorMsg.c_str());
        ImGui::SetCursorScreenPos(ImVec2(cX + (cW - eSize.x) * 0.5f, cY + 238.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.282f, 0.282f, 0.90f));
        ImGui::TextUnformatted(s_errorMsg.c_str());
        ImGui::PopStyleColor();
    }

    // Footer
    const char* footer = "BENZ EX  v1.2.0-PRO";
    ImVec2 fSize = ImGui::CalcTextSize(footer);
    ImGui::SetCursorScreenPos(ImVec2(cX + (cW - fSize.x) * 0.5f, cY + cH - 24.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.463f, 0.553f, 0.686f, 0.80f));
    ImGui::TextUnformatted(footer);
    ImGui::PopStyleColor();

    ImGui::End();
}

AuthState State() { return s_state; }

} // namespace KeyAuthUI
