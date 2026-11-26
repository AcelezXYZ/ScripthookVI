#pragma once
#include "backend.h"
#include "host.h"
#include <string>

namespace shvi::render {
// IRenderBackend over the C function table a host registers (shviRenderSetBackend).
class HostBackend final : public IRenderBackend {
public:
	explicit HostBackend(const ShviRenderBackendDesc& d) : m_d(d), m_name(d.name ? d.name : "host") {}
	const char* Name() const override { return m_name.c_str(); }
	ShviGraphicsApi Api() const override { return static_cast<ShviGraphicsApi>(m_d.api); }
	bool Install() override { return true; }
	void Uninstall() override {}
	bool CopyForHistory(const ShviResource& src, ShviResource& dst) override
	{
		return m_d.copyForHistory && m_d.copyForHistory(m_d.user, &src, &dst) != 0;
	}

private:
	ShviRenderBackendDesc m_d;
	std::string m_name;
};
} // namespace shvi::render
