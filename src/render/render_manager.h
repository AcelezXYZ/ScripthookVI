#pragma once
// Platform-neutral frame/history bookkeeping and callback dispatch for the
// render hooks. No Windows or graphics-API code in here (unit-testable).
#include "backend.h"
#include "render_types.h"
#include <mutex>
#include <vector>

namespace shvi::render {

using StageCallback = void (*)(ShviRenderStage, const ShviFrameInfo*);

class Manager {
public:
	static Manager& Get();

	// --- called by the plugin API ---
	void Register(ShviRenderStage stage, StageCallback cb);
	void Unregister(ShviRenderStage stage, StageCallback cb);
	bool GetFrameInfo(int history, ShviFrameInfo* out) const;
	bool GetResource(ShviResourceSlot slot, int history, ShviResource* out) const;
	int Api() const;

	// --- called by a backend, on the render thread ---
	void SetBackend(IRenderBackend* backend, bool allowNativeHandles);
	// Starts a frame. Sets info.resetHistory when previous-frame data must not
	// be reused: first frame, resolution change, frame index gap, or the
	// backend asked for it.
	void BeginFrame(ShviFrameInfo info);
	void Publish(const ShviResource& res); // engine-owned, valid this frame
	void Dispatch(ShviRenderStage stage);
	// Ends the frame: copies retained slots (colour, motion vectors, depth)
	// through the backend so they are available as "previous" next frame.
	void EndFrame();

	// Test hook: forget all state.
	void ResetForTests();

private:
	struct Record {
		ShviFrameInfo info{};
		ShviResource res[SHVI_RES_COUNT]{};
		bool valid = false;
	};
	bool InBounds(int history) const { return history == 0 || history == 1; }

	mutable std::mutex m_mutex;
	std::vector<StageCallback> m_callbacks[SHVI_STAGE_COUNT];
	IRenderBackend* m_backend = nullptr;
	bool m_allowNative = false;
	Record m_current;
	Record m_previous;
};
} // namespace shvi::render
