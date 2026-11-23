#include "native_invoker.h"
#include "../core/log.h"
#include "../game/game_api.h"
#include <cstring>
#include <mutex>
#include <unordered_set>

namespace shvi::natives {
namespace {
constexpr uint32_t kMaxArgs = 32;

// Per-thread call state so scripts on different threads cannot interleave.
struct CallState {
	uint64_t hash = 0;
	uint32_t argc = 0;
	uint64_t args[kMaxArgs] = {};
	uint64_t ret[8] = {}; // 64 bytes: enough for a Vector3 (3 slots) with spare
};
thread_local CallState t_state;

void LogMissingOnce(uint64_t hash)
{
	static std::mutex m;
	static std::unordered_set<uint64_t> seen;
	std::lock_guard lock(m);
	if (seen.insert(hash).second) SHVI_WARN("native 0x%016llX has no handler", (unsigned long long)hash);
}
} // namespace

void Init(uint64_t hash)
{
	t_state.hash = hash;
	t_state.argc = 0;
}

void Push(uint64_t value)
{
	if (t_state.argc >= kMaxArgs) {
		SHVI_ERROR("native 0x%016llX: too many arguments (max %u)", (unsigned long long)t_state.hash, kMaxArgs);
		return;
	}
	t_state.args[t_state.argc++] = value;
}

uint64_t* Call()
{
	CallState& s = t_state;
	std::memset(s.ret, 0, sizeof(s.ret));

	NativeHandler handler = game::GetNativeHandler(s.hash);
	if (!handler) {
		LogMissingOnce(s.hash);
		return s.ret;
	}

	NativeContext ctx{};
	ctx.returnValue = s.ret;
	ctx.args = s.args;
	ctx.argCount = s.argc;
	handler(&ctx);
	// TODO: copy vector results back from ctx.vectorSpace once the layout is known.
	s.argc = 0;
	return s.ret;
}
} // namespace shvi::natives
