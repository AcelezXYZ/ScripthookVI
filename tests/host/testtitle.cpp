// Stand-in "title" (our own code) for the end-to-end test. Provides what the
// hook needs from a real title: a per-frame function to hook, a native
// handler lookup, and a script-globals table. The test host finds them by RVA.
#include <windows.h>
#include <cstdint>

// Position-independent function with >= 15 bytes before any branch, so the
// 14-byte detour can steal 15 bytes. Returns a+b+c+d.
asm(R"(
.text
.globl title_tick
title_tick:
	movq %rcx, %rax
	addq %rdx, %rax
	addq %r8, %rax
	addq %r9, %rax
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	ret
)");
extern "C" uint64_t title_tick(uint64_t, uint64_t, uint64_t, uint64_t);

struct Ctx { // same layout as shvi::NativeContext
	uint64_t* ret; uint32_t argc, p0; uint64_t* args; uint32_t data, p1; uint8_t vec[192];
};

static void AddHandler(Ctx* c) { c->ret[0] = c->args[0] + c->args[1]; }

extern "C" void* title_get_native_handler(uint64_t hash)
{
	return hash == 0x1234 ? reinterpret_cast<void*>(&AddHandler) : nullptr;
}

static uint64_t g_page0[0x40000];
extern "C" uint64_t* title_global_pages[64];
uint64_t* title_global_pages[64];

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		g_page0[5] = 777;
		title_global_pages[0] = g_page0;
		(void)&title_tick; // keep it linked
	}
	return TRUE;
}
