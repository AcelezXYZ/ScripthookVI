#pragma once
#include <filesystem>

namespace shvi::paths {
// Folder of ScriptHookVI.ini / ScriptHookVI.log (default: folder of the DLL).
const std::filesystem::path& ConfigDir();
// Folder searched for *.asi plugins (default: folder of the host executable).
const std::filesystem::path& GameDir();
// File name of the host executable, e.g. "victory.exe".
const std::wstring& ExeName();
// Optional overrides; call before anything else uses the paths.
void Override(const std::filesystem::path& configDir, const std::filesystem::path& gameDir);
} // namespace shvi::paths
