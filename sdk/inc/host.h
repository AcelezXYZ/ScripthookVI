// ScriptHookVI - host interface.
//
// For the program that runs the game (e.g. VIctory), not for plugins. Load
// ScriptHookVI.dll (LoadLibrary), resolve these three functions with
// GetProcAddress and call shviAttach AFTER the title image is mapped and
// BEFORE its entry point runs, so hooks are in place before any title thread
// executes the hooked code. Do not call it from DllMain (it loads plugins).
//
// Plain C types only, so it can be bound from C, C++ or Rust.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define SHVI_HOST_EXTERN extern "C"
#else
#define SHVI_HOST_EXTERN
#endif
#ifdef SHVI_BUILD
#define SHVI_HOST_API SHVI_HOST_EXTERN __declspec(dllexport)
#else
#define SHVI_HOST_API SHVI_HOST_EXTERN __declspec(dllimport)
#endif

#define SHVI_HOST_API_VERSION 1

// The host guarantees the title cannot reach an online service (VIctory is
// offline by design). Then no OnlineFlag is required for attaching.
#define SHVI_HOST_OFFLINE 0x1u

typedef struct ShviAttachInfo {
	uint32_t structSize;      // sizeof(ShviAttachInfo)
	uint32_t flags;           // SHVI_HOST_*
	uintptr_t imageBase;      // base address of the mapped title image (PE headers intact)
	size_t imageSize;         // SizeOfImage
	const char* buildId;      // e.g. BLAKE3 hex of the original executable; may be NULL
	const wchar_t* configDir; // folder with ScriptHookVI.ini and for the log; NULL = folder of the DLL
	const wchar_t* gameDir;   // folder searched for plugins; NULL = folder of the host executable
} ShviAttachInfo;

// Returns 1 if scripts will be ticked (hooks installed), 0 otherwise. The
// reason is in ScriptHookVI.log. Plugins are loaded either way.
SHVI_HOST_API int shviAttach(const ShviAttachInfo* info);
// Stops ticking scripts. Hooks stay in place (removing them is not safe).
SHVI_HOST_API void shviDetach(void);
// Standalone mode for an ordinary Windows game: attach to the host
// executable itself (used by the dinput8 proxy).
SHVI_HOST_API int shviAttachMainModule(void);
