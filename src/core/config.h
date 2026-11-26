#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>

namespace shvi {
// Minimal INI reader: "[section]", "key = value", ';' and '#' comments.
// Keys are case-insensitive and addressed as ("Section", "Key").
class Config {
public:
	bool Load(const std::filesystem::path& file);
	std::string GetString(const std::string& section, const std::string& key,
	                      const std::string& def = {}) const;
	long long GetInt(const std::string& section, const std::string& key, long long def = 0) const;
	bool GetBool(const std::string& section, const std::string& key, bool def = false) const;

private:
	std::unordered_map<std::string, std::string> m_values;
};

Config& GlobalConfig();
} // namespace shvi
