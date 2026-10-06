#pragma once
#include "imgui.h"

namespace MenuUI {

struct Config {
    bool wantClose   = false;
    int  currentTheme = 0; // 0: Midnight Aurora, 1: Cyber Neon, 2: Stealth Dark

    bool espBox          = true;
    bool espName         = true;
    bool espDistance     = false;
    bool espLines        = false;
    bool espGranny       = true;
    bool espSlendrina    = true;
    bool espSpider       = true;
    bool espRat          = false;
    float espMaxDistance = 100.0f;
};

extern Config g_Config;

void Init();
void Shutdown();
void ApplyTheme(int themeIndex);
void Draw();
bool WantClose();

} // namespace MenuUI
