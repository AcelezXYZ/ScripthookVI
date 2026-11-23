#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace shvi::mem {

struct Region {
	uintptr_t base = 0;
	size_t size = 0;
};

// Image of the game executable.
Region MainModule();
// PE TimeDateStamp of the game executable (identifies the build).
uint32_t MainModuleTimestamp();

// Scan the readable sections of the image for an IDA-style pattern,
// e.g. "48 8B 05 ?? ?? ?? ?? 48 85 C0". Returns the first match or nullptr.
uint8_t* FindPattern(const Region& region, const std::string& pattern);

// Resolve a RIP-relative operand. `instr` points to the first byte of the
// instruction, `dispOffset` is the offset of the 32-bit displacement inside it
// and `instrLen` the full instruction length.
uint8_t* ResolveRip(uint8_t* instr, int dispOffset, int instrLen);

// x64 inline hook using a 14-byte absolute jump (FF 25 00000000 <addr64>).
//  * `stolenBytes` (>= 14) must end on an instruction boundary and the stolen
//    instructions must be position independent (no RIP-relative operands,
//    no relative jumps/calls); they are copied unchanged into the trampoline.
//  * Install hooks before the game's own threads run the target, or accept
//    the (small) race. Returns false on failure; *trampoline is the callable
//    "original".
bool InstallDetour(void* target, void* hook, size_t stolenBytes, void** trampoline);

bool IsReadable(const void* p, size_t len);
} // namespace shvi::mem
