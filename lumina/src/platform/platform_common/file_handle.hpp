#pragma once

#include <cstdint>

namespace lumina::platform::common {

// Platform-agnostic handle for file and console resources.
// Windows: stores a HANDLE (pointer) reinterpret_cast to uintptr_t
// Linux:   stores an int fd static_cast to uintptr_t
using FileHandle = uintptr_t;

// Sentinel for an invalid/failed handle.
// Maps to (uintptr_t)-1, consistent with INVALID_HANDLE_VALUE on Windows
// and the -1 error return from POSIX open/creat on Linux.
constexpr FileHandle InvalidFileHandle = static_cast<FileHandle>(-1);

} // namespace lumina::platform::common
