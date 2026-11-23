#pragma once
#include "../../sdk/inc/main.h"

namespace shvi::input {
void Register(KeyboardHandler h);
void Unregister(KeyboardHandler h);
// Finds the game's window thread and installs a WH_KEYBOARD hook on it.
// Blocks (polls) until a window is found or `timeoutMs` elapsed.
bool Install(DWORD timeoutMs);
void Uninstall();
} // namespace shvi::input
