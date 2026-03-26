#include "linux_window.hpp"

#include "vulkan/linux_vulkan.hpp"

#include "common/logger/logger.hpp"
#include "common/path_registry.hpp"
#include "common/scope_guard.hpp"
#include "common/timer.hpp"
#include "linux_platform_services.hpp"
#include "platform/platform_common/platform_services.hpp"
#include "platform/platform_common/runtime_root.hpp"

#include "core/lumina_engine.hpp"

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

  LOG_INFO("Logger initialized - Welcome to Lumina!");

  LOG_TRACE("Creating window...");
  auto window_result = Window::Create("Lumina");
  if (!window_result) {
    LOG_ERROR("Failed to create window: {}", window_result.error().message);
    return 1;
  }
  auto &window = window_result.value().get();
  ScopeGuard window_guard([]() -> void {
    LOG_TRACE("Destroying window...");
    Window::Destroy();
  });

  LOG_TRACE("Initializing Vulkan Linux Platform...");
  auto vulkan_init_result = llinux::vulkan::InitializeVulkan(window);
  if (!vulkan_init_result) {
    LOG_ERROR("Initailization failure: {}", vulkan_init_result.error().message);
    return 1;
  }

  auto &vulkan_init_res = vulkan_init_result.value();
  ScopeGuard vulkan_guard([&vulkan_init_res]() -> void {
    LOG_TRACE("Destroying Vulkan Linux Platform...");
    vulkan::DestroyVulkan(vulkan_init_res);
  }); // Destroy vulkan on exit

  auto window_dimensions = window.GetWindowClientDimensions();
  lumina::core::LuminaInitializeInfo init_info = {
      .vulkan_init_result = vulkan_init_res,
      .window_dimensions = window_dimensions,
  };

  LOG_TRACE("Initializing Lumina engine...");
  lumina::core::LuminaEngine::Initialize(init_info);
  auto &engine = lumina::core::LuminaEngine::Instance();

  Timer timer;
  timer.Reset();
  while (window.IsRunning()) {
    engine.GetInputState().NewFrame();
    window.ProcessMessages();
    window.ProcessMouseMovement();
    engine.BeginFrame(timer);
    engine.ExecuteFrame();
    engine.EndFrame();
  }

  LOG_TRACE("Shutting down Lumina engine...");
  lumina::core::LuminaEngine::Shutdown();

  return 0;
}