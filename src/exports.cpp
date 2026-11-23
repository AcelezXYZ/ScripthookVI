// Public C API (see sdk/inc/main.h).
#define SHVI_BUILD
#include "../sdk/inc/main.h"
#include "core/memory.h"
#include "game/game_api.h"
#include "runtime/input.h"
#include "runtime/native_invoker.h"
#include "runtime/script_manager.h"

using namespace shvi;

SHVI_API int shviGetApiVersion() { return SHVI_API_VERSION; }

SHVI_API void scriptWait(DWORD ms) { ScriptManager::Get().Wait(ms); }
SHVI_API void scriptRegister(HMODULE m, void (*fn)()) { ScriptManager::Get().Register(m, fn); }
SHVI_API void scriptRegisterAdditionalThread(HMODULE m, void (*fn)()) { ScriptManager::Get().Register(m, fn); }
SHVI_API void scriptUnregisterModule(HMODULE m) { ScriptManager::Get().UnregisterModule(m); }
SHVI_API void scriptUnregisterMain(void (*fn)()) { ScriptManager::Get().UnregisterMain(fn); }

SHVI_API void keyboardHandlerRegister(KeyboardHandler h) { input::Register(h); }
SHVI_API void keyboardHandlerUnregister(KeyboardHandler h) { input::Unregister(h); }

SHVI_API void nativeInit(UINT64 hash) { natives::Init(hash); }
SHVI_API void nativePush64(UINT64 v) { natives::Push(v); }
SHVI_API PUINT64 nativeCall() { return natives::Call(); }

SHVI_API UINT64* getGlobalPtr(int index) { return game::GlobalPtr(index); }
SHVI_API DWORD getGameExeTimestamp() { return mem::MainModuleTimestamp(); }
SHVI_API int shviIsAttached() { return game::IsAttached() ? 1 : 0; }
