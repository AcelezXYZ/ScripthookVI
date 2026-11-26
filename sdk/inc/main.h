// ScriptHookVI SDK - public API.
//
// A plugin is a DLL renamed to *.asi and placed next to the game executable
// (or in its "plugins" folder). Its DllMain registers a script main function;
// the hook runs it as a cooperative fiber on the game's script thread.
//
//   BOOL APIENTRY DllMain(HMODULE m, DWORD reason, LPVOID)
//   {
//       if (reason == DLL_PROCESS_ATTACH) scriptRegister(m, ScriptMain);
//       if (reason == DLL_PROCESS_DETACH) scriptUnregisterModule(m);
//       return TRUE;
//   }
//   void ScriptMain() { for (;;) { /* ... */ scriptWait(0); } }
#pragma once
#include <windows.h>
#include "types.h"

#ifdef SHVI_BUILD
#define SHVI_API extern "C" __declspec(dllexport)
#else
#define SHVI_API extern "C" __declspec(dllimport)
#endif

// ---- API version -----------------------------------------------------------
#define SHVI_API_VERSION 1
SHVI_API int shviGetApiVersion();

// ---- scripts ---------------------------------------------------------------
// Yield the calling script for at least `ms` milliseconds (0 = next tick).
// Must be called from a script fiber; a script that never waits stalls the game.
SHVI_API void scriptWait(DWORD ms);
// Register the main function of a script. Safe to call from DllMain.
SHVI_API void scriptRegister(HMODULE module, void (*scriptMain)());
// Additional script thread (fiber) for the same module.
SHVI_API void scriptRegisterAdditionalThread(HMODULE module, void (*scriptMain)());
// Remove all scripts of a module / one specific script. Call on DLL_PROCESS_DETACH.
SHVI_API void scriptUnregisterModule(HMODULE module);
SHVI_API void scriptUnregisterMain(void (*scriptMain)());

// ---- keyboard --------------------------------------------------------------
// Parameters mirror the WM_KEYDOWN / WM_KEYUP lParam bit fields.
typedef void (*KeyboardHandler)(DWORD key, WORD repeats, BYTE scanCode, BOOL isExtended,
                                BOOL isWithAlt, BOOL wasDownBefore, BOOL isUpNow);
SHVI_API void keyboardHandlerRegister(KeyboardHandler handler);
SHVI_API void keyboardHandlerUnregister(KeyboardHandler handler);

// ---- natives ---------------------------------------------------------------
// Prefer the typed invoke<>() helper in nativeCaller.h.
SHVI_API void nativeInit(UINT64 hash);
SHVI_API void nativePush64(UINT64 value);
SHVI_API PUINT64 nativeCall();

// ---- globals / game info ---------------------------------------------------
// Pointer to a script global, or nullptr if the global table is not resolved.
SHVI_API UINT64* getGlobalPtr(int index);
// PE TimeDateStamp of the title image; use it to pin per-build data.
SHVI_API DWORD getGameExeTimestamp();
// Build id the host passed to shviAttach (e.g. BLAKE3 hex), "" if none.
SHVI_API const char* getGameBuildId();
// 1 when the hook is fully attached (scripts are being ticked), 0 otherwise.
SHVI_API int shviIsAttached();
