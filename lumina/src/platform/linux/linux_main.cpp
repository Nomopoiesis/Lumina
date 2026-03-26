#include "linux_window.hpp"

#include "common/logger/logger.hpp"
#include "common/path_registry.hpp"
#include "common/scope_guard.hpp"
#include "linux_platform_services.hpp"
#include "platform/platform_common/platform_services.hpp"
#include "platform/platform_common/runtime_root.hpp"

auto main() -> int {
  using namespace lumina::platform::llinux;
  using namespace lumina::common;
  using namespace lumina::platform;

  // Initialize platform services before using them
  InitPlatformServices();

  // Initialize path registry rooted at the executable's directory
  PathRegistry::Initialize(common::GetRuntimeRoot());
  ScopeGuard path_registry_guard([]() -> void { PathRegistry::Shutdown(); });

  // Initialize logger; in this configuration we keep the console open
  // at shutdown until the user presses a key, so that validation and
  // other shutdown-time messages remain visible. We also enable ANSI
  // console colors by default for better log readability.
  logger::Logger::Initialize(logger::LogTargetType::Console,
                             common::PlatformServices::Instance(),
                             /*wait_for_keypress_on_shutdown*/ true,
                             /*enable_console_colors*/ true);

  LOG_TRACE("Creating window...");
  auto window_result = Window::Create();
  if (!window_result) {
    LOG_ERROR("Failed to create window: {}", window_result.error().message);
    return 1;
  }
  auto &window = window_result.value().get();
}