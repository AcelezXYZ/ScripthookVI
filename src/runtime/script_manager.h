#pragma once
#include <windows.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace shvi {

using ScriptMain = void (*)();

// Runs plugin scripts as cooperative fibers on the thread that calls Tick().
class ScriptManager {
public:
	static ScriptManager& Get();

	// Thread-safe, callable from DllMain: queues the script.
	void Register(HMODULE module, ScriptMain main);
	void UnregisterModule(HMODULE module);
	void UnregisterMain(ScriptMain main);

	// Game thread only. Runs every script whose wait time has elapsed.
	void Tick();
	// Called from inside a script fiber.
	void Wait(DWORD ms);

	void Shutdown();
	bool HasScripts() const;

private:
	struct Script {
		HMODULE module = nullptr;
		ScriptMain main = nullptr;
		LPVOID fiber = nullptr;
		ULONGLONG wakeAt = 0;
		bool dead = false;
		bool kill = false;
	};

	static void CALLBACK FiberEntry(LPVOID param);
	void EnsureMainFiber();

	mutable std::mutex m_mutex;
	std::vector<std::unique_ptr<Script>> m_scripts;
	LPVOID m_mainFiber = nullptr;
	Script* m_current = nullptr;
};
} // namespace shvi
