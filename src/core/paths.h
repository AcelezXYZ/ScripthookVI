#pragma once
#include <filesystem>

namespace shvi::paths {
// Directory that contains ScriptHookVI.dll (where the .ini and .log live).
const std::filesystem::path& ModuleDir();
// Directory of the game executable (where *.asi plugins are searched).
const std::filesystem::path& GameDir();
// File name of the game executable, e.g. "Game.exe".
const std::wstring& ExeName();
} // namespace shvi::paths
