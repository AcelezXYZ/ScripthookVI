// ScriptHookVI - entry point. Loaded into the game process by the
// dinput8.dll proxy (or any other injector). Initialisation runs on its own
// thread, never inside DllMain (loader lock).
#include "core/config.h"
#include "core/log.h"
#include "core/paths.h"
#include "game/game_api.h"
#include "render/render_init.h"
#include "runtime/asi_loader.h"
#include "runtime/input.h"
#include "version.h"
#include <windows.h>
#include <algorithm>

namespace {
bool ExeAllowed()
{
	std::string want = shvi::GlobalConfig().GetString("General", "TargetExe");
	if (want.empty()) return true; // no restriction configured
	std::wstring exe = shvi::paths::ExeName();
	std::wstring w(want.begin(), want.end());
	auto lower = [](std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(towlower(c)); return s; };
	return lower(w) == lower(exe);
}

DWORD WINAPI Bootstrap(LPVOID)
{
	using namespace shvi;
	log::Init(paths::ModuleDir() / L"ScriptHookVI.log");
	SHVI_INFO("ScriptHookVI %s starting in %s", SHVI_VERSION_STRING, paths::GameDir().string().c_str());

	if (!GlobalConfig().Load(paths::ModuleDir() / L"ScriptHookVI.ini"))
		SHVI_WARN("ScriptHookVI.ini not found, using defaults (no patterns -> no hook)");

	if (!GlobalConfig().GetBool("General", "Enabled", true)) { SHVI_INFO("disabled in ini"); return 0; }
	if (!ExeAllowed()) { SHVI_INFO("executable not in [General] TargetExe, idle"); return 0; }

	const bool attached = game::Init();
	render::Init();
	input::Install(static_cast<DWORD>(GlobalConfig().GetInt("General", "WindowTimeoutMs", 60000)));
	if (GlobalConfig().GetBool("Asi", "Load", true)) {
		int n = asi::LoadAll();
		SHVI_INFO("%d plugin(s) loaded", n);
	}
	SHVI_INFO(attached ? "ready" : "running WITHOUT game hook (see warnings above)");
	return 0;
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(module);
		HANDLE t = CreateThread(nullptr, 0, Bootstrap, nullptr, 0, nullptr);
		if (t) CloseHandle(t);
	} else if (reason == DLL_PROCESS_DETACH) {
		shvi::input::Uninstall();
		shvi::render::Shutdown();
		shvi::game::Shutdown();
	}
	return TRUE;
}
