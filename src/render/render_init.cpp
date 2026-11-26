#include "render_init.h"
#include "../core/config.h"
#include "../core/log.h"
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

void Shutdown()
{
	Manager::Get().SetBackend(nullptr, false);
	if (g_backend) g_backend->Uninstall();
	g_backend = nullptr;
}
} // namespace shvi::render
