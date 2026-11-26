// dinput8.dll proxy: forwards DirectInput8 to the system DLL and loads
// ScriptHookVI.dll from its own folder and attaches it to the host executable
// (standalone mode, for an ordinary Windows game; VIctory does not use this,
// it calls shviAttach itself). ASSUMPTION: the game imports
// dinput8.dll (as earlier RAGE titles do). If it does not, rename the proxy
// to another DLL the game loads (version.dll, winmm.dll, ...) and forward
// that API instead, or use any other injector for ScriptHookVI.dll.
#include <windows.h>
#include <string>

namespace {
HMODULE g_real = nullptr;

HMODULE Real()
{
	if (!g_real) {
		wchar_t dir[MAX_PATH];
		GetSystemDirectoryW(dir, MAX_PATH);
		std::wstring path = std::wstring(dir) + L"\\dinput8.dll";
		g_real = LoadLibraryW(path.c_str());
	}
	return g_real;
}
template <typename Fn> Fn Resolve(const char* name)
{
	return reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(Real(), name)));
}
} // namespace

extern "C" {
__declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID* out, LPUNKNOWN u)
{
	static auto fn = Resolve<HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN)>("DirectInput8Create");
	return fn ? fn(h, v, r, out, u) : E_FAIL;
}
__declspec(dllexport) HRESULT WINAPI DllCanUnloadNow() { return S_FALSE; }
__declspec(dllexport) HRESULT WINAPI DllGetClassObject(REFCLSID c, REFIID i, LPVOID* o)
{
	static auto fn = Resolve<HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*)>("DllGetClassObject");
	return fn ? fn(c, i, o) : E_FAIL;
}
__declspec(dllexport) HRESULT WINAPI DllRegisterServer() { return E_FAIL; }
__declspec(dllexport) HRESULT WINAPI DllUnregisterServer() { return E_FAIL; }
}

namespace {
DWORD WINAPI AttachThread(LPVOID lib)
{
	using Fn = int(__cdecl*)();
	auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(lib), "shviAttachMainModule")));
	if (fn) fn();
	return 0;
}
} // namespace

BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(self);
		wchar_t path[MAX_PATH];
		DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
		while (n && path[n - 1] != L'\\') --n;
		path[n] = L'\0';
		lstrcatW(path, L"ScriptHookVI.dll");
		if (HMODULE lib = LoadLibraryW(path)) {
			// Not inside DllMain: attaching loads plugins (loader lock).
			if (HANDLE t = CreateThread(nullptr, 0, AttachThread, lib, 0, nullptr)) CloseHandle(t);
		}
	}
	return TRUE;
}
