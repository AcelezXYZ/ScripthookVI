#include "paths.h"
#include <windows.h>

namespace shvi::paths {
namespace {
std::filesystem::path FromModule(HMODULE m)
{
	wchar_t buf[MAX_PATH * 2];
	DWORD n = GetModuleFileNameW(m, buf, static_cast<DWORD>(std::size(buf)));
	return std::filesystem::path(std::wstring(buf, n));
}

std::filesystem::path g_config;
std::filesystem::path g_game;

std::filesystem::path DefaultConfigDir()
{
	HMODULE self = nullptr;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                   reinterpret_cast<LPCWSTR>(&DefaultConfigDir), &self);
	return FromModule(self).parent_path();
}
} // namespace

const std::filesystem::path& ConfigDir()
{
	if (g_config.empty()) g_config = DefaultConfigDir();
	return g_config;
}

const std::filesystem::path& GameDir()
{
	if (g_game.empty()) g_game = FromModule(nullptr).parent_path();
	return g_game;
}

const std::wstring& ExeName()
{
	static const std::wstring name = FromModule(nullptr).filename().wstring();
	return name;
}

void Override(const std::filesystem::path& configDir, const std::filesystem::path& gameDir)
{
	if (!configDir.empty()) g_config = configDir;
	if (!gameDir.empty()) g_game = gameDir;
}
} // namespace shvi::paths
