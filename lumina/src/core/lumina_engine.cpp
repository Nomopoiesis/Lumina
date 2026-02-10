#include "lumina_engine.hpp"

#include "common/logger/logger.hpp"

namespace lumina::core {

auto LuminaEngine::Initialize(const LuminaInitializeInfo &init_info) -> void {
  auto &instance = GetStaticInstance();
  instance.window_dimensions = init_info.window_dimensions;
  instance.renderer =
      std::make_unique<renderer::LuminaRenderer>(init_info.vulkan_init_result);
  instance.renderer->Initialize();
  instance.is_initialized = true;
}

auto LuminaEngine::Shutdown() -> void {
  auto &instance = GetStaticInstance();
  instance.renderer->DeviceWaitIdle();
  instance.renderer.reset();
  instance.is_initialized = false;
}

auto LuminaEngine::ExecuteFrame() -> void { renderer->DrawFrame(); }

} // namespace lumina::core
