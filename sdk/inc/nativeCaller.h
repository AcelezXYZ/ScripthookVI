// ScriptHookVI SDK - typed wrapper over nativeInit / nativePush64 / nativeCall.
#pragma once
#include <type_traits>
#include "main.h"

template <typename T>
inline void nativePush(T val)
{
	static_assert(sizeof(T) <= sizeof(UINT64), "native arguments are at most 8 bytes");
	UINT64 slot = 0;
	*reinterpret_cast<T*>(&slot) = val;
	nativePush64(slot);
}

// invoke<Ret>(hash, args...)
//   Ped p = invoke<Ped>(0x1234ABCD1234ABCDull, 0);   // example only, not a real native
template <typename R, typename... Args>
inline R invoke(UINT64 hash, Args... args)
{
	nativeInit(hash);
	(nativePush(args), ...);
	if constexpr (std::is_void_v<R>) {
		nativeCall();
	} else {
		return *reinterpret_cast<R*>(nativeCall());
	}
}
