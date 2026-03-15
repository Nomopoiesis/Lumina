#pragma once

namespace lumina::platform::windows {

// Initializes the PlatformServices singleton with Windows-specific
// implementations for all file and console operations
auto InitPlatformServices() -> void;

} // namespace lumina::platform::windows
