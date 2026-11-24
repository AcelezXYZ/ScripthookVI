// ScriptHookVI SDK - render types. Plain C types only (no <windows.h>), so they
// can be shared with tools and tests.
#pragma once
#include <stdint.h>

// Points in a frame where plugins get called.
enum ShviRenderStage {
	SHVI_STAGE_PRE_UI = 0,  // scene fully rendered, HUD/UI NOT drawn yet
	SHVI_STAGE_POST_UI = 1, // after the UI pass
	SHVI_STAGE_PRESENT = 2, // right before the swapchain present
	SHVI_STAGE_COUNT
};

// Per-frame render targets a plugin can ask for.
enum ShviResourceSlot {
	SHVI_RES_COLOR = 0,          // scene colour BEFORE the UI pass
	SHVI_RES_MOTION_VECTORS = 1, // per-pixel motion, see ShviFrameInfo scale
	SHVI_RES_DEPTH = 2,
	SHVI_RES_COLOR_FINAL = 3,    // colour after the UI pass
	SHVI_RES_COUNT
};

enum ShviGraphicsApi { SHVI_API_NONE = 0, SHVI_API_D3D12 = 1, SHVI_API_VULKAN = 2 };

// Resources that are kept for one extra frame ("previous frame").
#define SHVI_HISTORY_CURRENT 0
#define SHVI_HISTORY_PREVIOUS 1

struct ShviFrameInfo {
	uint32_t structSize;      // sizeof(ShviFrameInfo), set by the caller of Get*
	uint32_t api;             // ShviGraphicsApi
	uint64_t frameIndex;      // increments by 1 per rendered frame
	uint32_t renderWidth;     // internal (pre-upscale) resolution
	uint32_t renderHeight;
	float jitterX;            // sub-pixel camera jitter in pixels
	float jitterY;
	float motionVectorScaleX; // multiply MV texels by this to get pixels
	float motionVectorScaleY;
	float viewProj[16];       // row-major, this frame
	float prevViewProj[16];   // row-major, previous frame
	int32_t resetHistory;     // 1: do not reuse previous-frame data (cut, resize, first frame)
	uint32_t _pad;
};

struct ShviResource {
	uint32_t structSize; // sizeof(ShviResource)
	uint32_t slot;       // ShviResourceSlot
	uint32_t valid;      // 0 = not available (never published / not retained)
	uint32_t width;
	uint32_t height;
	uint32_t format;     // DXGI_FORMAT or VkFormat value, see ShviFrameInfo::api
	uint64_t frameIndex; // frame this data belongs to
	// ID3D12Resource* or VkImage. NULL unless [Render] AllowNativeHandles=1.
	void* nativeHandle;
};
