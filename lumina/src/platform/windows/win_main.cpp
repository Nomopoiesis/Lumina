#include <Windows.h>
#include <timeapi.h> // timeBeginPeriod / timeEndPeriod

#include "scope_guard.hpp"
#include "vulkan/win_vulkan.hpp"
#include "win_window.hpp"

#include "core/lumina_engine.hpp"

#include "common/logger/logger.hpp"
#include "common/timer.hpp"
#include "platform/common/platform_services.hpp"
#include "win_platform_services.hpp"

auto WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nShowCmd) -> int {
  using namespace lumina::platform::windows;
  using namespace lumina::common;
  using namespace lumina::platform;

  // Set timer resolution to 1ms for better sleep precision
  timeBeginPeriod(1);
  // Restore timer resolution on exit
  ScopeGuard time_period_guard([]() { timeEndPeriod(1); });

  // Initialize platform services before using them
  InitPlatformServices();

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
  auto window_result = Window::Create(hInstance, "Lumina", 800, 600);
  if (!window_result) {
    LOG_ERROR("Failed to create window: {}", window_result.error().message);
    return 1;
  }
  auto &window = window_result.value().get();
  ScopeGuard window_guard([]() -> void {
    LOG_TRACE("Destroying window...");
    Window::Destroy();
  });

  LOG_TRACE("Initializing Vulkan Windows Platform...");
  auto vulkan_init_result = windows::vulkan::InitializeVulkan(window);
  if (!vulkan_init_result) {
    LOG_ERROR("Initailization failure: {}", vulkan_init_result.error().message);
    return 1;
  }

  auto &vulkan_init_res = vulkan_init_result.value();
  ScopeGuard vulkan_guard([&vulkan_init_res]() -> void {
    LOG_TRACE("Destroying Vulkan Windows Platform...");
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
    auto delta_time = timer.Tick();
    engine.GetInputState().NewFrame();
    window.ProcessMessages();
    window.ProcessMouseMovement();
    engine.ExecuteFrame(delta_time);
  }

  LOG_TRACE("Shutting down Lumina engine...");
  lumina::core::LuminaEngine::Shutdown();

  return 0;
}
