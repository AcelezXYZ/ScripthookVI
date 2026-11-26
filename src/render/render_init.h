#pragma once
namespace shvi::render {
// Reads [Render] from the ini and installs a graphics backend if one exists.
// Returns true when render hooks are active.
bool Init();
void Shutdown();
} // namespace shvi::render
