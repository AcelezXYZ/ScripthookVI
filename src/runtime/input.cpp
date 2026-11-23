#include "input.h"
#include "../core/log.h"
#include <algorithm>
#include <mutex>
#include <vector>

namespace shvi::input {
namespace {
std::mutex g_mutex;
std::vector<KeyboardHandler> g_handlers;
HHOOK g_hook = nullptr;

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION) {
		// lParam layout is identical to WM_KEYDOWN / WM_KEYUP.
		const WORD repeats = static_cast<WORD>(lParam & 0xFFFF);
		const BYTE scan = static_cast<BYTE>((lParam >> 16) & 0xFF);
		const BOOL ext = (lParam >> 24) & 1;
		const BOOL alt = (lParam >> 29) & 1;
		const BOOL was = (lParam >> 30) & 1;
		const BOOL up = (lParam >> 31) & 1;

		std::vector<KeyboardHandler> copy;
		{
			std::lock_guard lock(g_mutex);
			copy = g_handlers;
		}
		for (auto h : copy) h(static_cast<DWORD>(wParam), repeats, scan, ext, alt, was, up);
	}
	return CallNextHookEx(g_hook, code, wParam, lParam);
}

struct Find {
	DWORD pid;
	DWORD thread = 0;
};

BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lp)
{
	auto* f = reinterpret_cast<Find*>(lp);
	DWORD pid = 0;
	DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
	if (pid == f->pid && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
		f->thread = tid;
		return FALSE;
	}
	return TRUE;
}
} // namespace

void Register(KeyboardHandler h)
{
	if (!h) return;
	std::lock_guard lock(g_mutex);
	if (std::find(g_handlers.begin(), g_handlers.end(), h) == g_handlers.end()) g_handlers.push_back(h);
}

void Unregister(KeyboardHandler h)
{
	std::lock_guard lock(g_mutex);
	g_handlers.erase(std::remove(g_handlers.begin(), g_handlers.end(), h), g_handlers.end());
}

bool Install(DWORD timeoutMs)
{
	Find f{GetCurrentProcessId()};
	const ULONGLONG deadline = GetTickCount64() + timeoutMs;
	while (!f.thread && GetTickCount64() < deadline) {
		EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&f));
		if (!f.thread) Sleep(250);
	}
	if (!f.thread) {
		SHVI_WARN("no game window found; keyboard handlers disabled");
		return false;
	}
	g_hook = SetWindowsHookExW(WH_KEYBOARD, KeyboardProc, nullptr, f.thread);
	if (!g_hook) {
		SHVI_ERROR("SetWindowsHookEx failed (error %lu)", GetLastError());
		return false;
	}
	SHVI_INFO("keyboard hook installed on thread %lu", f.thread);
	return true;
}

void Uninstall()
{
	if (g_hook) UnhookWindowsHookEx(g_hook);
	g_hook = nullptr;
}
} // namespace shvi::input
