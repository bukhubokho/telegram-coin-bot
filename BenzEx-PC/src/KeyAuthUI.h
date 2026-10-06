#pragma once
#include <string>

namespace KeyAuthUI {

enum class AuthState {
    Idle,       // waiting for user input
    Checking,   // HTTP request in flight
    Success,    // key accepted
    Failed      // key rejected / network error
};

void Init();
void Draw();            // call every frame until State() == Success
AuthState State();
std::string HwidGet();  // expose for reuse / display

} // namespace KeyAuthUI
