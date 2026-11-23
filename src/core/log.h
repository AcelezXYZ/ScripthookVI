#pragma once
#include <filesystem>

namespace shvi::log {
void Init(const std::filesystem::path& file);
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
void Write(const char* level, const char* fmt, ...);
} // namespace shvi::log

#define SHVI_INFO(...) ::shvi::log::Write("INFO", __VA_ARGS__)
#define SHVI_WARN(...) ::shvi::log::Write("WARN", __VA_ARGS__)
#define SHVI_ERROR(...) ::shvi::log::Write("ERROR", __VA_ARGS__)
