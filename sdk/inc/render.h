// ScriptHookVI SDK - render hooks (SCAFFOLD).
//
// For render extensions (upscalers, frame generation, overlays). Callbacks run
// on the game's render thread; keep them short. Frame data is only valid
// inside a callback, except history index 1 which is a backend-owned copy that
// stays valid for exactly one more frame.
//
// Nothing is delivered until a graphics backend is implemented and its
// patterns are configured (see README, "Render hooks").
#pragma once
#include "main.h"
#include "render_types.h"

typedef void (*RenderStageCallback)(ShviRenderStage stage, const ShviFrameInfo* frame);

SHVI_API void renderCallbackRegister(ShviRenderStage stage, RenderStageCallback cb);
SHVI_API void renderCallbackUnregister(ShviRenderStage stage, RenderStageCallback cb);

// history: SHVI_HISTORY_CURRENT or SHVI_HISTORY_PREVIOUS.
// Fills *out and returns 1, or returns 0 if that data is not available.
SHVI_API int renderGetFrameInfo(int history, ShviFrameInfo* out);
SHVI_API int renderGetResource(ShviResourceSlot slot, int history, ShviResource* out);

// ShviGraphicsApi of the active backend (SHVI_API_NONE if none).
SHVI_API int renderGetApi();
