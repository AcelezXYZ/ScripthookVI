#include "render_manager.h"
#include <algorithm>
#include <cstring>

namespace shvi::render {

Manager& Manager::Get()
{
	static Manager inst;
	return inst;
}

void Manager::Register(ShviRenderStage stage, StageCallback cb)
{
	if (!cb || stage < 0 || stage >= SHVI_STAGE_COUNT) return;
	std::lock_guard lock(m_mutex);
	auto& v = m_callbacks[stage];
	if (std::find(v.begin(), v.end(), cb) == v.end()) v.push_back(cb);
}

void Manager::Unregister(ShviRenderStage stage, StageCallback cb)
{
	if (stage < 0 || stage >= SHVI_STAGE_COUNT) return;
	std::lock_guard lock(m_mutex);
	auto& v = m_callbacks[stage];
	v.erase(std::remove(v.begin(), v.end(), cb), v.end());
}

int Manager::Api() const
{
	std::lock_guard lock(m_mutex);
	return m_backend ? static_cast<int>(m_backend->Api()) : SHVI_API_NONE;
}

void Manager::SetBackend(IRenderBackend* backend, bool allowNativeHandles)
{
	std::lock_guard lock(m_mutex);
	m_backend = backend;
	m_allowNative = allowNativeHandles;
}

bool Manager::GetFrameInfo(int history, ShviFrameInfo* out) const
{
	if (!out || !InBounds(history)) return false;
	std::lock_guard lock(m_mutex);
	const Record& r = history == SHVI_HISTORY_CURRENT ? m_current : m_previous;
	if (!r.valid) return false;
	*out = r.info;
	out->structSize = sizeof(ShviFrameInfo);
	return true;
}

bool Manager::GetResource(ShviResourceSlot slot, int history, ShviResource* out) const
{
	if (!out || slot < 0 || slot >= SHVI_RES_COUNT || !InBounds(history)) return false;
	std::lock_guard lock(m_mutex);
	const Record& r = history == SHVI_HISTORY_CURRENT ? m_current : m_previous;
	if (!r.valid || !r.res[slot].valid) return false;
	*out = r.res[slot];
	out->structSize = sizeof(ShviResource);
	// Raw API handles are opt-in; plugins get sizes/formats either way.
	if (!m_allowNative) out->nativeHandle = nullptr;
	return true;
}

void Manager::BeginFrame(ShviFrameInfo info)
{
	std::lock_guard lock(m_mutex);
	const Record& prev = m_previous;
	bool reset = info.resetHistory != 0;
	if (!prev.valid) reset = true;
	else if (prev.info.renderWidth != info.renderWidth || prev.info.renderHeight != info.renderHeight) reset = true;
	else if (info.frameIndex != prev.info.frameIndex + 1) reset = true;

	info.resetHistory = reset ? 1 : 0;
	info.structSize = sizeof(ShviFrameInfo);
	m_current = Record{};
	m_current.info = info;
	m_current.valid = true;
}

void Manager::Publish(const ShviResource& res)
{
	if (res.slot >= SHVI_RES_COUNT) return;
	std::lock_guard lock(m_mutex);
	if (!m_current.valid) return;
	m_current.res[res.slot] = res;
	m_current.res[res.slot].structSize = sizeof(ShviResource);
	m_current.res[res.slot].valid = 1;
	m_current.res[res.slot].frameIndex = m_current.info.frameIndex;
}

void Manager::Dispatch(ShviRenderStage stage)
{
	if (stage < 0 || stage >= SHVI_STAGE_COUNT) return;
	std::vector<StageCallback> cbs;
	ShviFrameInfo info;
	{
		std::lock_guard lock(m_mutex);
		if (!m_current.valid) return;
		cbs = m_callbacks[stage];
		info = m_current.info;
	}
	// Called without the lock so callbacks may use the Get* functions.
	for (auto cb : cbs) cb(stage, &info);
}

void Manager::EndFrame()
{
	std::lock_guard lock(m_mutex);
	if (!m_current.valid) return;

	Record next;
	next.info = m_current.info;
	next.valid = true;
	static constexpr ShviResourceSlot kRetained[] = {SHVI_RES_COLOR, SHVI_RES_MOTION_VECTORS, SHVI_RES_DEPTH};
	for (ShviResourceSlot slot : kRetained) {
		const ShviResource& src = m_current.res[slot];
		if (!src.valid || !m_backend) continue;
		ShviResource copy{};
		if (m_backend->CopyForHistory(src, copy) && copy.valid) {
			copy.slot = slot;
			copy.frameIndex = src.frameIndex;
			next.res[slot] = copy; // slots that could not be copied stay invalid
		}
	}
	m_previous = next;
	m_current = Record{};
}

void Manager::ResetForTests()
{
	std::lock_guard lock(m_mutex);
	for (auto& v : m_callbacks) v.clear();
	m_backend = nullptr;
	m_allowNative = false;
	m_current = Record{};
	m_previous = Record{};
}
} // namespace shvi::render
