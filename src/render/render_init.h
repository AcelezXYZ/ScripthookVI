#pragma once
#include "host.h"

namespace shvi::render {
// Reads [Render] from the ini and installs a graphics backend if one exists.
// Returns true when render hooks are active.
bool Init();
void Shutdown();
// Used by shviRenderSetBackend: installs a host-provided backend if [Render]
// Enabled=1. Returns true when active.
bool SetHostBackend(const ShviRenderBackendDesc& desc);
} // namespace shvi::render
