#include "platform/platform_common/runtime_root.hpp"

// WIN32_LEAN_AND_MEAN and NOMINMAX come from the windows_lumina target in
// lumina/src/platform/windows/CMakeLists.txt.
#include <Windows.h>

#include <filesystem>

namespace lumina::platform::common {

auto GetRuntimeRoot() -> std::filesystem::path {
  char buf[MAX_PATH];
  GetModuleFileNameA(nullptr, buf, MAX_PATH);
  return std::filesystem::path(buf).parent_path();
}

} // namespace lumina::platform::common
