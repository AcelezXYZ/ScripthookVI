// ScriptHookVI - DLL entry. Nothing happens on load: a host (or the dinput8
// proxy) calls shviAttach / shviAttachMainModule, see sdk/inc/host.h.
#include "core/attach.h"
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(module);
	} else if (reason == DLL_PROCESS_DETACH && !reserved) {
		shvi::core::Detach(); // reserved != NULL means process exit: skip
	}
	return TRUE;
}
