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
} // namespace

const std::filesystem::path& ModuleDir()
{
	static const std::filesystem::path dir = [] {
		HMODULE self = nullptr;
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		                   reinterpret_cast<LPCWSTR>(&ModuleDir), &self);
		return FromModule(self).parent_path();
	}();
	return dir;
}

const std::filesystem::path& GameDir()
{
	static const std::filesystem::path dir = FromModule(nullptr).parent_path();
	return dir;
}

const std::wstring& ExeName()
{
	static const std::wstring name = FromModule(nullptr).filename().wstring();
	return name;
}
} // namespace shvi::paths
