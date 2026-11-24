// Public C API (see sdk/inc/main.h).
#define SHVI_BUILD
#include "../sdk/inc/main.h"
#include "../sdk/inc/render.h"
#include "core/memory.h"
#include "game/game_api.h"
#include "render/render_manager.h"
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

// ---- render hooks (scaffold, see sdk/inc/render.h) ----
SHVI_API void renderCallbackRegister(ShviRenderStage s, RenderStageCallback cb) { render::Manager::Get().Register(s, cb); }
SHVI_API void renderCallbackUnregister(ShviRenderStage s, RenderStageCallback cb) { render::Manager::Get().Unregister(s, cb); }
SHVI_API int renderGetFrameInfo(int history, ShviFrameInfo* out) { return render::Manager::Get().GetFrameInfo(history, out) ? 1 : 0; }
SHVI_API int renderGetResource(ShviResourceSlot slot, int history, ShviResource* out) { return render::Manager::Get().GetResource(slot, history, out) ? 1 : 0; }
SHVI_API int renderGetApi() { return render::Manager::Get().Api(); }
