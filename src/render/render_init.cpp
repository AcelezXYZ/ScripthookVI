#include "render_init.h"
#include "../core/config.h"
#include "../core/log.h"
#include "host_backend.h"
#include "render_manager.h"

namespace shvi::render {
namespace {
IRenderBackend* g_backend = nullptr;

// Backends (D3D12, Vulkan) are not implemented yet. A backend would be
// created here based on [Render] Api and the imports of the game executable.
IRenderBackend* CreateBackend(const std::string& /*api*/) { return nullptr; }
} // namespace

bool Init()
{
	const Config& cfg = GlobalConfig();
	if (!cfg.GetBool("Render", "Enabled", false)) return false;

	g_backend = CreateBackend(cfg.GetString("Render", "Api", "auto"));
	if (!g_backend) {
		SHVI_WARN("[Render] Enabled=1 but no graphics backend is implemented yet; render hooks inactive");
		return false;
	}
	if (!g_backend->Install()) {
		SHVI_ERROR("render backend %s failed to install", g_backend->Name());
		g_backend = nullptr;
		return false;
	}
	Manager::Get().SetBackend(g_backend, cfg.GetBool("Render", "AllowNativeHandles", false));
	SHVI_INFO("render backend %s active", g_backend->Name());
	return true;
}

bool SetHostBackend(const ShviRenderBackendDesc& desc)
{
	const Config& cfg = GlobalConfig();
	if (!cfg.GetBool("Render", "Enabled", false)) return false;
	if (desc.structSize < sizeof(ShviRenderBackendDesc) || !desc.copyForHistory) {
		SHVI_ERROR("host render backend: invalid description");
		return false;
	}
	static HostBackend* backend = nullptr; // lives for the process
	delete backend;
	backend = new HostBackend(desc);
	g_backend = backend;
	const bool native = cfg.GetBool("Render", "AllowNativeHandles", false);
	Manager::Get().SetBackend(backend, native);
	SHVI_INFO("render backend \"%s\" (host) active, native handles for plugins: %s", backend->Name(),
	          native ? "yes" : "no");
	return true;
}

void Shutdown()
{
	Manager::Get().SetBackend(nullptr, false);
	g_backend = nullptr; // the host backend object is kept: a host thread may still be inside it
}
} // namespace shvi::render
