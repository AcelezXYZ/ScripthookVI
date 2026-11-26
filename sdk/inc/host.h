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
#include "render_types.h"

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

// ---- render feed ------------------------------------------------------------
// Lets the host feed frames to plugins that registered render callbacks (see
// render.h). Only active when ScriptHookVI.ini has [Render] Enabled=1.
//
// Per frame, on the render thread:
//   shviRenderBeginFrame(info);
//   shviRenderPublish(res) for colour (before the UI pass), motion vectors, depth;
//   shviRenderStage(SHVI_STAGE_PRE_UI);  ... the host's upscaler runs ...
//   shviRenderEndFrame();   // retained slots are copied through copyForHistory
//
// ShviResource::nativeHandle is the API object (ID3D12Resource*). Plugins only
// see it with [Render] AllowNativeHandles=1. Matrices in ShviFrameInfo may be
// zero when the host cannot provide them.
typedef struct ShviRenderBackendDesc {
	uint32_t structSize; // sizeof(ShviRenderBackendDesc)
	uint32_t api;        // ShviGraphicsApi
	const char* name;    // for the log
	void* user;          // passed back to copyForHistory
	// Copy `src` (valid for this frame only) into a host-owned resource that
	// stays valid through the next frame; fill *dst (valid=1, size, format,
	// nativeHandle). Return 1 on success, 0 if it cannot be copied.
	int (*copyForHistory)(void* user, const ShviResource* src, ShviResource* dst);
} ShviRenderBackendDesc;

// Returns 1 if the render feed is active ([Render] Enabled=1 and attached).
SHVI_HOST_API int shviRenderSetBackend(const ShviRenderBackendDesc* desc);
SHVI_HOST_API void shviRenderBeginFrame(const ShviFrameInfo* info);
SHVI_HOST_API void shviRenderPublish(const ShviResource* resource);
SHVI_HOST_API void shviRenderStage(int stage); // ShviRenderStage
SHVI_HOST_API void shviRenderEndFrame(void);
