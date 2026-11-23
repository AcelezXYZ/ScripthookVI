// Smallest possible ScriptHookVI plugin. Build target "hello" -> hello.asi.
#include "main.h"
#include "nativeCaller.h"
#include <cstdio>

static bool g_enabled = true;

static void OnKey(DWORD key, WORD, BYTE, BOOL, BOOL, BOOL wasDown, BOOL isUp)
{
	if (key == VK_F9 && !isUp && !wasDown) g_enabled = !g_enabled;
}

static void ScriptMain()
{
	DWORD ticks = 0;
	for (;;) {
		if (g_enabled && ++ticks % 10 == 0) {
			char msg[64];
			snprintf(msg, sizeof(msg), "hello.asi alive, %lu ticks\n", ticks);
			OutputDebugStringA(msg);
			// Natives go through invoke<>(hash, args...). Hashes are not known
			// for GTA VI yet, so there is no real call here:
			//   invoke<void>(0x0000000000000000ull /* hash */, arg1, arg2);
		}
		scriptWait(100);
	}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		scriptRegister(module, ScriptMain);
		keyboardHandlerRegister(OnKey);
	} else if (reason == DLL_PROCESS_DETACH) {
		keyboardHandlerUnregister(OnKey);
		scriptUnregisterModule(module);
	}
	return TRUE;
}
