// End-to-end test host: plays the role of VIctory. Maps a stand-in title,
// loads ScriptHookVI.dll, attaches it, runs the title's per-frame function on
// a "game thread" and checks what the plugin script saw.
//
//   testhost pass         everything configured, host is offline  -> attached, scripts run
//   testhost nopatterns   no targets in the ini                   -> not attached, no scripts
//   testhost guard        targets set but host not offline, no    -> not attached (online guard)
//                         OnlineFlag
//   testhost badimage     attach with a bogus image               -> not attached, no crash
#include "host.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

struct Status { volatile LONG ticks, nativeOk, globalOk, waitOk, keyHandlerSet; };
using TickFn = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t);

static int g_failed = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("  FAIL: %s\n", msg); ++g_failed; } else std::printf("  ok:   %s\n", msg); } while (0)

int main(int argc, char** argv)
{
	const std::string mode = argc > 1 ? argv[1] : "pass";

	wchar_t exe[MAX_PATH];
	GetModuleFileNameW(nullptr, exe, MAX_PATH);
	std::wstring dir(exe);
	dir.resize(dir.find_last_of(L'\\') + 1);
	SetCurrentDirectoryW(dir.c_str());

	HMODULE title = LoadLibraryW(L"testtitle.dll");
	HMODULE shvi = LoadLibraryW(L"ScriptHookVI.dll");
	if (!title || !shvi) { std::printf("cannot load test DLLs (%p %p)\n", (void*)title, (void*)shvi); return 2; }

	auto base = reinterpret_cast<uintptr_t>(title);
	auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
	auto rva = [&](const char* sym) { return reinterpret_cast<uintptr_t>(GetProcAddress(title, sym)) - base; };
	auto tick = reinterpret_cast<TickFn>(GetProcAddress(title, "title_tick"));
	if (!tick) { std::printf("title_tick not exported\n"); return 2; }

	// ini
	std::string ini = "[General]\nWindowTimeoutMs=0\n[Asi]\nDirectories=.\n";
	char buf[512];
	if (mode != "nopatterns") {
		std::snprintf(buf, sizeof(buf),
		              "[Patterns]\nScriptTick.Rva=0x%llX\nScriptTick.Stolen=15\n"
		              "GetNativeHandler.Rva=0x%llX\nGlobalBase.Rva=0x%llX\n",
		              (unsigned long long)rva("title_tick"), (unsigned long long)rva("title_get_native_handler"),
		              (unsigned long long)rva("title_global_pages"));
		ini += buf;
	}
	std::string iniPath(dir.begin(), dir.end());
	if (FILE* f = std::fopen((iniPath + "ScriptHookVI.ini").c_str(), "w")) { std::fputs(ini.c_str(), f); std::fclose(f); }

	using AttachFn = int (*)(const ShviAttachInfo*);
	auto attach = reinterpret_cast<AttachFn>(reinterpret_cast<void*>(GetProcAddress(shvi, "shviAttach")));
	if (!attach) { std::printf("shviAttach not exported\n"); return 2; }

	ShviAttachInfo info{};
	info.structSize = sizeof(info);
	info.flags = mode == "guard" ? 0 : SHVI_HOST_OFFLINE;
	info.imageBase = base;
	info.imageSize = nt->OptionalHeader.SizeOfImage;
	info.buildId = "test-build";
	info.configDir = dir.c_str();
	info.gameDir = dir.c_str();
	if (mode == "badimage") { info.imageBase = 0x10000; info.imageSize = 0x1000; }

	std::printf("[%s]\n", mode.c_str());
	const int attached = attach(&info);

	const bool expectAttached = mode == "pass";
	CHECK(attached == (expectAttached ? 1 : 0), expectAttached ? "attach returns 1" : "attach returns 0");

	// "Game thread": the title's frame function, 200 frames, 10 ms apart.
	int wrong = 0;
	for (int i = 0; i < 200; ++i) {
		if (tick(1, 2, 3, 4) != 10) ++wrong;
		Sleep(10);
	}
	CHECK(wrong == 0, "title_tick still returns the original result (hook passes through)");

	HMODULE plugin = GetModuleHandleW(L"selftest.asi");
	CHECK(plugin != nullptr, "plugin selftest.asi was loaded");
	using StatusFn = const Status* (*)();
	auto status = plugin ? reinterpret_cast<StatusFn>(reinterpret_cast<void*>(GetProcAddress(plugin, "selftest_status"))) : nullptr;
	if (status) {
		const Status* s = status();
		std::printf("  ticks=%ld native=%ld global=%ld wait=%ld\n", s->ticks, s->nativeOk, s->globalOk, s->waitOk);
		if (expectAttached) {
			CHECK(s->ticks >= 10 && s->ticks <= 100, "script ticked, and scriptWait(50) throttled it (not once per frame)");
			CHECK(s->nativeOk == 1, "native call 0x1234(40, 2) == 42");
			CHECK(s->globalOk == 1, "getGlobalPtr(5) == 777");
			CHECK(s->waitOk == 1, "scriptWait waited >= 45 ms");
		} else {
			CHECK(s->ticks == 0, "no script ran");
		}
	}

	if (expectAttached) {
		using BuildFn = const char* (*)();
		auto build = reinterpret_cast<BuildFn>(reinterpret_cast<void*>(GetProcAddress(shvi, "getGameBuildId")));
		CHECK(build && std::strcmp(build(), "test-build") == 0, "getGameBuildId returns the host's build id");
	}

	using DetachFn = void (*)();
	if (auto d = reinterpret_cast<DetachFn>(reinterpret_cast<void*>(GetProcAddress(shvi, "shviDetach")))) d();
	std::printf(g_failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", g_failed);
	return g_failed ? 1 : 0;
}
