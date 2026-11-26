#include "config.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

namespace shvi {
namespace {
std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
	return s;
}
std::string Trim(const std::string& s)
{
	size_t b = 0, e = s.size();
	while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
	while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
	return s.substr(b, e - b);
}
std::string MakeKey(const std::string& section, const std::string& key)
{
	return Lower(section) + "." + Lower(key);
}
} // namespace

bool Config::Load(const std::filesystem::path& file)
{
	std::ifstream in(file);
	if (!in) return false;

	std::string line, section;
	while (std::getline(in, line)) {
		// Strip comments. A ';' or '#' starts a comment anywhere on the line.
		size_t c = line.find_first_of(";#");
		if (c != std::string::npos) line.erase(c);
		line = Trim(line);
		if (line.empty()) continue;
		if (line.front() == '[' && line.back() == ']') {
			section = Trim(line.substr(1, line.size() - 2));
			continue;
		}
		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		m_values[MakeKey(section, Trim(line.substr(0, eq)))] = Trim(line.substr(eq + 1));
	}
	return true;
}

std::string Config::GetString(const std::string& section, const std::string& key,
                              const std::string& def) const
{
	auto it = m_values.find(MakeKey(section, key));
	return it == m_values.end() ? def : it->second;
}

long long Config::GetInt(const std::string& section, const std::string& key, long long def) const
{
	auto it = m_values.find(MakeKey(section, key));
	if (it == m_values.end() || it->second.empty()) return def;
	return std::strtoll(it->second.c_str(), nullptr, 0); // accepts 0x.. hex
}

bool Config::GetBool(const std::string& section, const std::string& key, bool def) const
{
	auto it = m_values.find(MakeKey(section, key));
	if (it == m_values.end() || it->second.empty()) return def;
	std::string v = Lower(it->second);
	return v == "1" || v == "true" || v == "yes" || v == "on";
}

Config& GlobalConfig()
{
	static Config cfg;
	return cfg;
}
} // namespace shvi
