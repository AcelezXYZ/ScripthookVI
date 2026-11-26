// Test plugin (selftest.asi): runs as a script and records what worked.
#include "main.h"
#include "nativeCaller.h"

struct Status { volatile LONG ticks, nativeOk, globalOk, waitOk, keyHandlerSet; };
static Status g_status;

extern "C" __declspec(dllexport) const Status* selftest_status() { return &g_status; }

static void OnKey(DWORD, WORD, BYTE, BOOL, BOOL, BOOL, BOOL) {}

static void ScriptMain()
{
	for (;;) {
		InterlockedIncrement(&g_status.ticks);

		if (invoke<UINT64>(0x1234ull, 40ull, 2ull) == 42) g_status.nativeOk = 1;
		const UINT64* g = getGlobalPtr(5);
		if (g && *g == 777) g_status.globalOk = 1;

		const ULONGLONG t0 = GetTickCount64();
		scriptWait(50);
		if (GetTickCount64() - t0 >= 45) g_status.waitOk = 1;
	}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		scriptRegister(module, ScriptMain);
		keyboardHandlerRegister(OnKey);
		g_status.keyHandlerSet = 1;
	} else if (reason == DLL_PROCESS_DETACH) {
		keyboardHandlerUnregister(OnKey);
		scriptUnregisterModule(module);
	}
	return TRUE;
}
