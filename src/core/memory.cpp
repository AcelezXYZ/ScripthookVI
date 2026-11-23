#include "memory.h"
#include "log.h"
#include <windows.h>
#include <cstring>
#include <vector>

namespace shvi::mem {

Region MainModule()
{
	auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
	auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
	return {base, nt->OptionalHeader.SizeOfImage};
}

uint32_t MainModuleTimestamp()
{
	auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
	auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
	return nt->FileHeader.TimeDateStamp;
}

bool IsReadable(const void* p, size_t len)
{
	MEMORY_BASIC_INFORMATION mbi;
	if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
	if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
	auto end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
	return reinterpret_cast<uintptr_t>(p) + len <= end;
}

namespace {
struct Pattern {
	std::vector<uint8_t> bytes;
	std::vector<bool> mask; // true = must match
};

bool Parse(const std::string& text, Pattern& out)
{
	size_t i = 0;
	while (i < text.size()) {
		while (i < text.size() && text[i] == ' ') ++i;
		if (i >= text.size()) break;
		size_t j = i;
		while (j < text.size() && text[j] != ' ') ++j;
		std::string tok = text.substr(i, j - i);
		i = j;
		if (tok == "?" || tok == "??") {
			out.bytes.push_back(0);
			out.mask.push_back(false);
		} else {
			char* end = nullptr;
			unsigned long v = std::strtoul(tok.c_str(), &end, 16);
			if (*end != '\0' || tok.size() > 2) return false;
			out.bytes.push_back(static_cast<uint8_t>(v));
			out.mask.push_back(true);
		}
	}
	return !out.bytes.empty();
}

bool Match(const uint8_t* p, const Pattern& pat)
{
	for (size_t k = 0; k < pat.bytes.size(); ++k)
		if (pat.mask[k] && p[k] != pat.bytes[k]) return false;
	return true;
}
} // namespace

uint8_t* FindPattern(const Region& region, const std::string& pattern)
{
	Pattern pat;
	if (!Parse(pattern, pat)) {
		SHVI_ERROR("invalid pattern: \"%s\"", pattern.c_str());
		return nullptr;
	}
	auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(region.base);
	auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(region.base + dos->e_lfanew);
	auto sec = IMAGE_FIRST_SECTION(nt);

	for (unsigned s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++sec) {
		if (!(sec->Characteristics & IMAGE_SCN_MEM_READ)) continue;
		if (sec->Characteristics & IMAGE_SCN_MEM_DISCARDABLE) continue;
		auto start = reinterpret_cast<uint8_t*>(region.base + sec->VirtualAddress);
		size_t len = sec->Misc.VirtualSize;
		if (len < pat.bytes.size()) continue;
		for (size_t off = 0; off + pat.bytes.size() <= len; ++off) {
			// Check readability once per page to survive sections with holes.
			if ((off & 0xFFF) == 0 && !IsReadable(start + off, 1)) {
				off += 0xFFF;
				continue;
			}
			if (Match(start + off, pat)) return start + off;
		}
	}
	return nullptr;
}

uint8_t* ResolveRip(uint8_t* instr, int dispOffset, int instrLen)
{
	if (!instr) return nullptr;
	int32_t disp;
	std::memcpy(&disp, instr + dispOffset, sizeof(disp));
	return instr + instrLen + disp;
}

bool InstallDetour(void* target, void* hook, size_t stolen, void** trampoline)
{
	constexpr size_t kJump = 14;
	if (!target || !hook || !trampoline || stolen < kJump || stolen > 64) return false;

	auto writeJump = [](uint8_t* at, const void* dest) {
		const uint8_t op[6] = {0xFF, 0x25, 0, 0, 0, 0};
		std::memcpy(at, op, sizeof(op));
		std::memcpy(at + 6, &dest, sizeof(dest));
	};

	auto* tramp = static_cast<uint8_t*>(
	    VirtualAlloc(nullptr, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
	if (!tramp) return false;

	std::memcpy(tramp, target, stolen);
	writeJump(tramp + stolen, static_cast<uint8_t*>(target) + stolen);

	DWORD old;
	if (!VirtualProtect(target, stolen, PAGE_EXECUTE_READWRITE, &old)) {
		VirtualFree(tramp, 0, MEM_RELEASE);
		return false;
	}
	writeJump(static_cast<uint8_t*>(target), hook);
	if (stolen > kJump) std::memset(static_cast<uint8_t*>(target) + kJump, 0x90, stolen - kJump);
	VirtualProtect(target, stolen, old, &old);
	FlushInstructionCache(GetCurrentProcess(), target, stolen);
	FlushInstructionCache(GetCurrentProcess(), tramp, 128);

	*trampoline = tramp;
	return true;
}
} // namespace shvi::mem
