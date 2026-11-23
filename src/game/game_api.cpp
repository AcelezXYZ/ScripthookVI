#include "game_api.h"
#include "../core/config.h"
#include "../core/log.h"
#include "../core/memory.h"
#include "../runtime/script_manager.h"
#include <atomic>
#include <windows.h>

namespace shvi::game {
namespace {

// ---- resolved game state -------------------------------------------------
using GetNativeHandlerFn = NativeHandler (*)(uint64_t hash);
using TickFn = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t);

GetNativeHandlerFn g_getNativeHandler = nullptr;
uint64_t** g_globalPages = nullptr; // [64][0x40000] layout, ASSUMPTION from earlier RAGE titles
const uint8_t* g_onlineFlag = nullptr;
TickFn g_originalTick = nullptr;

std::atomic<bool> g_attached{false};
std::atomic<DWORD> g_tickThread{0};
bool g_allowOnline = false;

// ---- pattern resolution --------------------------------------------------
// Keys under [Patterns], <Name> being e.g. "ScriptTick":
//   <Name>.Pattern  IDA-style byte pattern, empty = not configured
//   <Name>.Offset   bytes added to the match address (may be negative)
//   <Name>.RipDisp  offset of a rel32 displacement inside the instruction at
//   <Name>.RipLen   ... and that instruction's length; RipLen=0 = no RIP resolve
//   <Name>.Deref    1 = read a pointer at the resolved address
//   <Name>.Stolen   (hooks only) bytes to steal, >= 14, on an instruction boundary
uint8_t* ResolveTarget(const char* name, size_t* stolen = nullptr)
{
	const Config& cfg = GlobalConfig();
	const std::string key(name);
	std::string pattern = cfg.GetString("Patterns", key + ".Pattern");
	if (pattern.empty()) {
		SHVI_WARN("pattern %s: not configured", name);
		return nullptr;
	}
	uint8_t* hit = mem::FindPattern(mem::MainModule(), pattern);
	if (!hit) {
		SHVI_ERROR("pattern %s: not found in this build", name);
		return nullptr;
	}
	hit += cfg.GetInt("Patterns", key + ".Offset", 0);
	if (long len = cfg.GetInt("Patterns", key + ".RipLen", 0); len > 0)
		hit = mem::ResolveRip(hit, static_cast<int>(cfg.GetInt("Patterns", key + ".RipDisp", 0)), static_cast<int>(len));
	if (cfg.GetBool("Patterns", key + ".Deref", false)) {
		if (!mem::IsReadable(hit, sizeof(void*))) return nullptr;
		hit = *reinterpret_cast<uint8_t**>(hit);
	}
	if (stolen) *stolen = static_cast<size_t>(cfg.GetInt("Patterns", key + ".Stolen", 0));
	SHVI_INFO("pattern %s: resolved to %p (module+0x%llX)", name, (void*)hit,
	          (unsigned long long)(reinterpret_cast<uintptr_t>(hit) - mem::MainModule().base));
	return hit;
}

// ---- the tick hook -------------------------------------------------------
void OnTick()
{
	// Only ever tick from ONE thread, even if the hooked function is
	// entered by several.
	const DWORD tid = GetCurrentThreadId();
	DWORD expected = 0;
	g_tickThread.compare_exchange_strong(expected, tid);
	if (g_tickThread.load() != tid) return;

	if (!g_allowOnline && IsOnline()) return; // scripts are paused while online
	ScriptManager::Get().Tick();
}

uint64_t HookedTick(uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
	// Pass the first four integer-class arguments through unchanged and
	// return RAX. ASSUMPTION: the hooked function takes <= 4 integer args.
	const uint64_t r = g_originalTick(a, b, c, d);
	OnTick();
	return r;
}
} // namespace

bool Init()
{
	g_allowOnline = GlobalConfig().GetBool("Safety", "AllowOnline", false);
	SHVI_INFO("game exe timestamp 0x%08X", mem::MainModuleTimestamp());

	g_getNativeHandler = reinterpret_cast<GetNativeHandlerFn>(ResolveTarget("GetNativeHandler"));
	g_globalPages = reinterpret_cast<uint64_t**>(ResolveTarget("GlobalBase"));
	g_onlineFlag = ResolveTarget("OnlineFlag");

	if (!g_allowOnline && !g_onlineFlag &&
	    GlobalConfig().GetBool("Safety", "RequireOnlineGuard", true)) {
		SHVI_ERROR("OnlineFlag is unresolved and [Safety] RequireOnlineGuard=1: not attaching");
		return false;
	}

	size_t stolen = 0;
	void* tick = ResolveTarget("ScriptTick", &stolen);
	if (!tick) {
		SHVI_ERROR("ScriptTick unresolved: hook NOT attached, scripts will not run");
		return false;
	}
	if (!mem::InstallDetour(tick, reinterpret_cast<void*>(&HookedTick), stolen,
	                        reinterpret_cast<void**>(&g_originalTick))) {
		SHVI_ERROR("ScriptTick: detour failed (Stolen=%llu)", (unsigned long long)stolen);
		return false;
	}
	g_attached = true;
	SHVI_INFO("ScriptTick hooked, scripts will run");
	return true;
}

void Shutdown()
{
	// The detour is intentionally left in place: removing it while a game
	// thread may be inside the hook is not safe. Scripts just stop ticking.
	g_attached = false;
	ScriptManager::Get().Shutdown();
}

bool IsAttached() { return g_attached; }

NativeHandler GetNativeHandler(uint64_t hash)
{
	return g_getNativeHandler ? g_getNativeHandler(hash) : nullptr;
}

uint64_t* GlobalPtr(int index)
{
	if (!g_globalPages || index < 0) return nullptr;
	uint64_t* page = g_globalPages[(index >> 18) & 0x3F];
	return page ? &page[index & 0x3FFFF] : nullptr;
}

bool IsOnline()
{
	return g_onlineFlag && mem::IsReadable(g_onlineFlag, 1) && *g_onlineFlag != 0;
}
} // namespace shvi::game
