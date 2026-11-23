#include "log.h"
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace shvi::log {
namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
} // namespace

void Init(const std::filesystem::path& file)
{
	std::lock_guard lock(g_mutex);
	if (g_file) return;
	g_file = _wfopen(file.c_str(), L"w");
}

void Write(const char* level, const char* fmt, ...)
{
	std::lock_guard lock(g_mutex);
	if (!g_file) return;

	SYSTEMTIME t;
	GetLocalTime(&t);
	fprintf(g_file, "[%02d:%02d:%02d.%03d] [%-5s] ", t.wHour, t.wMinute, t.wSecond,
	        t.wMilliseconds, level);
	va_list args;
	va_start(args, fmt);
	vfprintf(g_file, fmt, args);
	va_end(args);
	fputc('\n', g_file);
	fflush(g_file);
}
} // namespace shvi::log
