#pragma once
#include "../runtime/native_invoker.h"
#include <cstdint>

// Everything that depends on the game's internals lives behind this header.
// All addresses are resolved from byte patterns that are configured in
// ScriptHookVI.ini ([Patterns]); the framework ships with NO patterns because
// none have been verified for GTA VI yet.
namespace shvi::game {

// Resolves the configured patterns and installs the script-tick hook.
// Returns true when scripts will be ticked.
bool Init();
void Shutdown();

bool IsAttached();
// Native hash -> handler, or nullptr if unresolved / unknown.
NativeHandler GetNativeHandler(uint64_t hash);
// &globals[index], or nullptr if the global table is unresolved.
uint64_t* GlobalPtr(int index);
// True while the game reports an online session. See Safety in the README.
bool IsOnline();
} // namespace shvi::game
