#include "attach.h"
#include "../game/game_api.h"
#include "../render/render_init.h"
#include "../runtime/asi_loader.h"
#include "../runtime/input.h"
#include "../version.h"
#include "config.h"
#include "log.h"
#include "paths.h"
#include <windows.h>
#include <mutex>

namespace shvi::core {
namespace {
std::mutex g_mutex;
bool g_done = false;
bool g_result = false;
std::string g_buildId;

DWORD WINAPI InputThread(LPVOID arg)
{
	input::Install(static_cast<DWORD>(reinterpret_cast<uintptr_t>(arg)));
	return 0;
}
} // namespace

bool Attach(const AttachParams& p)
{
	std::lock_guard lock(g_mutex);
	if (g_done) return g_result;
	g_done = true;

	paths::Override(p.configDir, p.gameDir);
	log::Init(paths::ConfigDir() / L"ScriptHookVI.log");
	SHVI_INFO("ScriptHookVI %s, %s mode", SHVI_VERSION_STRING, p.standalone ? "standalone" : "host");
	g_buildId = p.buildId;
	if (!g_buildId.empty()) SHVI_INFO("build id %s", g_buildId.c_str());

	if (!GlobalConfig().Load(paths::ConfigDir() / L"ScriptHookVI.ini"))
		SHVI_WARN("ScriptHookVI.ini not found in %s, using defaults (no targets -> no hook)",
		          paths::ConfigDir().string().c_str());
	if (!GlobalConfig().GetBool("General", "Enabled", true)) {
		SHVI_INFO("disabled in ini");
		return false;
	}

	if (p.standalone) {
		// Only in standalone mode: restrict to one executable name.
		const std::string want = GlobalConfig().GetString("General", "TargetExe");
		if (!want.empty()) {
			std::wstring w(want.begin(), want.end()), exe = paths::ExeName();
			for (auto& c : w) c = static_cast<wchar_t>(towlower(c));
			for (auto& c : exe) c = static_cast<wchar_t>(towlower(c));
			if (w != exe) {
				SHVI_INFO("executable is not [General] TargetExe, idle");
				return false;
			}
		}
	}

	if (mem::IsValidImage(p.image)) {
		mem::SetTargetImage(p.image);
		g_result = game::Init(p.hostIsOffline);
	} else {
		SHVI_ERROR("attach: image %p (%llu bytes) is not a readable PE64 image; no hook", (void*)p.image.base,
		           (unsigned long long)p.image.size);
		g_result = false;
	}

	const long timeout = static_cast<long>(GlobalConfig().GetInt("General", "WindowTimeoutMs", 60000));
	if (timeout > 0) {
		// Waits for the game window on its own thread so attach returns at once.
		HANDLE t = CreateThread(nullptr, 0, InputThread, reinterpret_cast<LPVOID>(static_cast<uintptr_t>(timeout)), 0, nullptr);
		if (t) CloseHandle(t);
	}
	if (GlobalConfig().GetBool("Asi", "Load", true)) SHVI_INFO("%d plugin(s) loaded", asi::LoadAll());
	render::Init();

	SHVI_INFO(g_result ? "ready: scripts will run" : "NOT attached: scripts will not run (see above)");
	return g_result;
}

void Detach()
{
	std::lock_guard lock(g_mutex);
	if (!g_done) return;
	input::Uninstall();
	render::Shutdown();
	game::Shutdown();
}

const std::string& BuildId() { return g_buildId; }
} // namespace shvi::core
