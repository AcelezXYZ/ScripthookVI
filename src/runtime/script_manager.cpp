#include "script_manager.h"
#include "../core/log.h"

namespace shvi {

ScriptManager& ScriptManager::Get()
{
	static ScriptManager inst;
	return inst;
}

void ScriptManager::Register(HMODULE module, ScriptMain main)
{
	if (!main) return;
	std::lock_guard lock(m_mutex);
	for (auto& s : m_scripts)
		if (s->main == main && !s->kill) return; // already registered
	auto s = std::make_unique<Script>();
	s->module = module;
	s->main = main;
	m_scripts.push_back(std::move(s));
	SHVI_INFO("script registered (module %p, main %p)", (void*)module, (void*)main);
}

void ScriptManager::UnregisterModule(HMODULE module)
{
	std::lock_guard lock(m_mutex);
	for (auto& s : m_scripts)
		if (s->module == module) s->kill = true;
}

void ScriptManager::UnregisterMain(ScriptMain main)
{
	std::lock_guard lock(m_mutex);
	for (auto& s : m_scripts)
		if (s->main == main) s->kill = true;
}

bool ScriptManager::HasScripts() const
{
	std::lock_guard lock(m_mutex);
	return !m_scripts.empty();
}

void ScriptManager::EnsureMainFiber()
{
	if (m_mainFiber) return;
	m_mainFiber = ConvertThreadToFiber(nullptr);
	// The game may already use fibers on this thread: reuse its fiber then.
	if (!m_mainFiber && GetLastError() == ERROR_ALREADY_FIBER) m_mainFiber = GetCurrentFiber();
	if (!m_mainFiber) SHVI_ERROR("could not obtain a main fiber (error %lu)", GetLastError());
}

void CALLBACK ScriptManager::FiberEntry(LPVOID param)
{
	auto* self = &Get();
	auto* s = static_cast<Script*>(param);
	try {
		s->main();
	} catch (...) {
		SHVI_ERROR("script %p threw an exception and was stopped", (void*)s->main);
	}
	s->dead = true;
	// A fiber function must never return (that would end the thread).
	for (;;) SwitchToFiber(self->m_mainFiber);
}

void ScriptManager::Wait(DWORD ms)
{
	if (!m_current || !m_mainFiber) return; // not inside a script fiber
	m_current->wakeAt = GetTickCount64() + ms;
	SwitchToFiber(m_mainFiber);
}

void ScriptManager::Tick()
{
	EnsureMainFiber();
	if (!m_mainFiber) return;

	// Snapshot raw pointers: scripts may register others while running.
	std::vector<Script*> run;
	{
		std::lock_guard lock(m_mutex);
		for (auto& s : m_scripts) run.push_back(s.get());
	}

	const ULONGLONG now = GetTickCount64();
	for (Script* s : run) {
		if (s->kill || s->dead) continue;
		if (s->wakeAt > now) continue;
		if (!s->fiber) {
			s->fiber = CreateFiber(0, &ScriptManager::FiberEntry, s);
			if (!s->fiber) {
				SHVI_ERROR("CreateFiber failed (error %lu)", GetLastError());
				s->dead = true;
				continue;
			}
		}
		m_current = s;
		SwitchToFiber(s->fiber);
		m_current = nullptr;
	}

	// Reap finished / killed scripts (we are on the main fiber here).
	std::lock_guard lock(m_mutex);
	for (auto it = m_scripts.begin(); it != m_scripts.end();) {
		Script* s = it->get();
		if (s->dead || s->kill) {
			if (s->fiber) DeleteFiber(s->fiber); // stack freed, C++ destructors not run
			SHVI_INFO("script %p removed (%s)", (void*)s->main, s->dead ? "finished" : "unregistered");
			it = m_scripts.erase(it);
		} else {
			++it;
		}
	}
}

void ScriptManager::Shutdown()
{
	std::lock_guard lock(m_mutex);
	for (auto& s : m_scripts) s->kill = true;
}
} // namespace shvi
