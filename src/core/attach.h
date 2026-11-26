#pragma once
#include "memory.h"
#include <filesystem>
#include <string>

namespace shvi::core {
struct AttachParams {
	mem::Region image;
	std::string buildId;
	bool hostIsOffline = false;
	std::filesystem::path configDir; // empty = default
	std::filesystem::path gameDir;   // empty = default
	bool standalone = false;         // attached to the host executable itself
};

// Initialises log + config, installs hooks, loads plugins. Returns true if
// scripts will be ticked. Only the first call does anything.
bool Attach(const AttachParams& params);
void Detach();
const std::string& BuildId();
} // namespace shvi::core
