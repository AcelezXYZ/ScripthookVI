#include "asi_loader.h"
#include "../core/config.h"
#include "../core/log.h"
#include "../core/paths.h"
#include <windows.h>
#include <sstream>

namespace shvi::asi {
int LoadAll()
{
	namespace fs = std::filesystem;
	std::istringstream dirs(GlobalConfig().GetString("Asi", "Directories", ". plugins"));
	int loaded = 0;
	std::string d;
	while (dirs >> d) {
		fs::path dir = paths::GameDir() / d;
		std::error_code ec;
		if (!fs::is_directory(dir, ec)) continue;
		for (const auto& e : fs::directory_iterator(dir, ec)) {
			if (!e.is_regular_file()) continue;
			auto ext = e.path().extension().wstring();
			for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
			if (ext != L".asi") continue;
			HMODULE m = LoadLibraryW(e.path().c_str());
			if (m) {
				++loaded;
				SHVI_INFO("loaded plugin %s", e.path().filename().string().c_str());
			} else {
				SHVI_ERROR("failed to load %s (error %lu)", e.path().filename().string().c_str(), GetLastError());
			}
		}
	}
	return loaded;
}
} // namespace shvi::asi
