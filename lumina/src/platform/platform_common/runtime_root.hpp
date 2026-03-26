#pragma once

#include <filesystem>

namespace lumina::platform::common {

// Returns the directory containing the running executable.
// Used to root the PathRegistry at the installed runtime data location.
[[nodiscard]] auto GetRuntimeRoot() -> std::filesystem::path;

} // namespace lumina::platform::common
