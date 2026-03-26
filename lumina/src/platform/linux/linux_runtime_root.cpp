#include "logger/logger.hpp"
#include "lumina_terminate.hpp"
#include "platform/platform_common/runtime_root.hpp"

#include <filesystem>

namespace lumina::platform::common {

auto GetRuntimeRoot() -> std::filesystem::path {
  char result[PATH_MAX];
  // readlink does not add a null terminator automatically
  ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
  if (count == -1) {
    LOG_CRITICAL("Could not get path to binary...");
    LUMINA_TERMINATE();
  }
  result[count] = '\0'; // Manually add null terminator
  return std::filesystem::path(result).parent_path();
}

} // namespace lumina::platform::common
