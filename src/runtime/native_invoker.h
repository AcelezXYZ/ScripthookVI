#pragma once
#include <cstdint>

namespace shvi {

// Argument/return block handed to a native handler.
// NOTE: this mirrors the layout used by earlier RAGE titles and is an
// ASSUMPTION for GTA VI until verified against the game (see README).
struct alignas(16) NativeContext {
	uint64_t* returnValue;
	uint32_t argCount;
	uint32_t _pad0;
	uint64_t* args;
	uint32_t dataCount;
	uint32_t _pad1;
	uint8_t vectorSpace[192];
};

using NativeHandler = void (*)(NativeContext*);

namespace natives {
void Init(uint64_t hash);
void Push(uint64_t value);
uint64_t* Call();
} // namespace natives
} // namespace shvi
