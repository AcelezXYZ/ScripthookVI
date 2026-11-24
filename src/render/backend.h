#pragma once
#include "render_types.h"

namespace shvi::render {

// One implementation per graphics API (D3D12, Vulkan). NOT implemented yet.
// A backend hooks the API (swapchain present, command lists), finds the
// pre-UI boundary, and feeds the manager:
//   Manager::BeginFrame -> Publish(slot...) -> Dispatch(PRE_UI)
//   -> Dispatch(POST_UI) -> Dispatch(PRESENT) -> Manager::EndFrame
class IRenderBackend {
public:
	virtual ~IRenderBackend() = default;
	virtual const char* Name() const = 0;
	virtual ShviGraphicsApi Api() const = 0;
	virtual bool Install() = 0;
	virtual void Uninstall() = 0;
	// Copy `src` (engine-owned, valid this frame only) into a backend-owned
	// resource that stays valid through the next frame. Return false if the
	// copy cannot be made; the slot is then unavailable as "previous".
	virtual bool CopyForHistory(const ShviResource& src, ShviResource& dst) = 0;
};
} // namespace shvi::render
