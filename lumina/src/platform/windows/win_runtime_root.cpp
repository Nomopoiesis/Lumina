#include "platform/common/runtime_root.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>

namespace lumina::platform::common {

auto GetRuntimeRoot() -> std::filesystem::path {
  char buf[MAX_PATH];
  GetModuleFileNameA(nullptr, buf, MAX_PATH);
  return std::filesystem::path(buf).parent_path();
}

} // namespace lumina::platform::common
