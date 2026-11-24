// Platform-neutral test of the render frame/history logic.
//   g++ -std=c++20 -Isdk/inc -Isrc/render tests/render_manager_test.cpp src/render/render_manager.cpp
#include "render_manager.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace shvi::render;

#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)

namespace {
struct FakeBackend : IRenderBackend {
	bool failDepth = false;
	int copies = 0;
	const char* Name() const override { return "fake"; }
	ShviGraphicsApi Api() const override { return SHVI_API_D3D12; }
	bool Install() override { return true; }
	void Uninstall() override {}
	bool CopyForHistory(const ShviResource& src, ShviResource& dst) override {
		if (failDepth && src.slot == SHVI_RES_DEPTH) return false;
		++copies;
		dst = src;
		dst.valid = 1;
		dst.nativeHandle = reinterpret_cast<void*>(0xC0FFEE); // backend-owned copy
		return true;
	}
};

ShviResource Res(ShviResourceSlot s, uint32_t w = 1920, uint32_t h = 1080)
{
	ShviResource r{};
	r.slot = s; r.width = w; r.height = h; r.format = 7;
	r.nativeHandle = reinterpret_cast<void*>(0x1000 + s);
	return r;
}

ShviFrameInfo Info(uint64_t idx, uint32_t w = 1920, uint32_t h = 1080)
{
	ShviFrameInfo i{};
	i.api = SHVI_API_D3D12; i.frameIndex = idx; i.renderWidth = w; i.renderHeight = h;
	return i;
}

void RunFrame(Manager& m, uint64_t idx, uint32_t w = 1920, uint32_t h = 1080)
{
	m.BeginFrame(Info(idx, w, h));
	m.Publish(Res(SHVI_RES_COLOR, w, h));
	m.Publish(Res(SHVI_RES_MOTION_VECTORS, w, h));
	m.Publish(Res(SHVI_RES_DEPTH, w, h));
	m.Dispatch(SHVI_STAGE_PRE_UI);
	m.Dispatch(SHVI_STAGE_POST_UI);
	m.Dispatch(SHVI_STAGE_PRESENT);
	m.EndFrame();
}

std::vector<int> g_order;
void CbPre(ShviRenderStage s, const ShviFrameInfo*) { g_order.push_back(100 + s); }
void CbAll(ShviRenderStage s, const ShviFrameInfo*) { g_order.push_back(200 + s); }
} // namespace

int main()
{
	Manager& m = Manager::Get();
	FakeBackend be;

	// 1. first frame: reset requested, no previous data
	m.ResetForTests(); m.SetBackend(&be, false);
	m.BeginFrame(Info(10));
	ShviFrameInfo fi{};
	CHECK(m.GetFrameInfo(SHVI_HISTORY_CURRENT, &fi) && fi.resetHistory == 1);
	CHECK(!m.GetFrameInfo(SHVI_HISTORY_PREVIOUS, &fi));
	m.EndFrame();

	// 2. consecutive frame: previous is available, reset cleared
	RunFrame(m, 11);
	m.BeginFrame(Info(12));
	CHECK(m.GetFrameInfo(SHVI_HISTORY_CURRENT, &fi) && fi.resetHistory == 0);
	CHECK(m.GetFrameInfo(SHVI_HISTORY_PREVIOUS, &fi) && fi.frameIndex == 11);
	ShviResource r{};
	CHECK(m.GetResource(SHVI_RES_COLOR, SHVI_HISTORY_PREVIOUS, &r) && r.frameIndex == 11);
	CHECK(m.GetResource(SHVI_RES_MOTION_VECTORS, SHVI_HISTORY_PREVIOUS, &r));
	CHECK(!m.GetResource(SHVI_RES_COLOR_FINAL, SHVI_HISTORY_PREVIOUS, &r)); // not retained
	CHECK(!m.GetResource(SHVI_RES_COLOR, SHVI_HISTORY_CURRENT, &r));        // not published yet
	m.EndFrame();

	// 3. resolution change and frame gap force a reset
	RunFrame(m, 13, 1280, 720);
	m.BeginFrame(Info(14, 1920, 1080));
	CHECK(m.GetFrameInfo(SHVI_HISTORY_CURRENT, &fi) && fi.resetHistory == 1);
	m.EndFrame();
	m.BeginFrame(Info(20, 1920, 1080));
	CHECK(m.GetFrameInfo(SHVI_HISTORY_CURRENT, &fi) && fi.resetHistory == 1);
	m.EndFrame();

	// 4. backend requested reset is kept
	RunFrame(m, 21);
	ShviFrameInfo cut = Info(22); cut.resetHistory = 1;
	m.BeginFrame(cut);
	CHECK(m.GetFrameInfo(SHVI_HISTORY_CURRENT, &fi) && fi.resetHistory == 1);
	m.EndFrame();

	// 5. native handles are hidden unless allowed
	RunFrame(m, 23);
	m.BeginFrame(Info(24)); m.Publish(Res(SHVI_RES_COLOR));
	CHECK(m.GetResource(SHVI_RES_COLOR, SHVI_HISTORY_CURRENT, &r) && r.nativeHandle == nullptr);
	CHECK(m.GetResource(SHVI_RES_COLOR, SHVI_HISTORY_PREVIOUS, &r) && r.nativeHandle == nullptr);
	m.EndFrame();
	m.SetBackend(&be, true);
	RunFrame(m, 25);
	m.BeginFrame(Info(26)); m.Publish(Res(SHVI_RES_COLOR));
	CHECK(m.GetResource(SHVI_RES_COLOR, SHVI_HISTORY_CURRENT, &r) && r.nativeHandle != nullptr);
	CHECK(m.GetResource(SHVI_RES_COLOR, SHVI_HISTORY_PREVIOUS, &r) && r.nativeHandle == reinterpret_cast<void*>(0xC0FFEE));
	m.EndFrame();

	// 6. a failed copy leaves only that slot unavailable as "previous"
	be.failDepth = true;
	RunFrame(m, 27);
	m.BeginFrame(Info(28));
	CHECK(m.GetResource(SHVI_RES_COLOR, SHVI_HISTORY_PREVIOUS, &r));
	CHECK(!m.GetResource(SHVI_RES_DEPTH, SHVI_HISTORY_PREVIOUS, &r));
	m.EndFrame();

	// 7. callbacks: per stage, dedup, unregister
	m.ResetForTests(); m.SetBackend(&be, false);
	m.Register(SHVI_STAGE_PRE_UI, CbPre);
	m.Register(SHVI_STAGE_PRE_UI, CbPre); // duplicate ignored
	m.Register(SHVI_STAGE_PRESENT, CbAll);
	g_order.clear();
	RunFrame(m, 1);
	CHECK((g_order == std::vector<int>{100 + SHVI_STAGE_PRE_UI, 200 + SHVI_STAGE_PRESENT}));
	m.Unregister(SHVI_STAGE_PRE_UI, CbPre);
	g_order.clear();
	RunFrame(m, 2);
	CHECK((g_order == std::vector<int>{200 + SHVI_STAGE_PRESENT}));

	// 8. no backend: no history, no crash
	m.ResetForTests();
	RunFrame(m, 1);
	m.BeginFrame(Info(2));
	CHECK(!m.GetResource(SHVI_RES_COLOR, SHVI_HISTORY_PREVIOUS, &r));
	CHECK(m.Api() == SHVI_API_NONE);
	m.EndFrame();

	std::puts("render_manager_test: all checks passed");
	return 0;
}
