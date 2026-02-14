#pragma once

#include <memory>
#include <stdexcept>

#include "platform/common/vulkan/vulkan_init_result.hpp"

#include "job_system/job_manager.hpp"
#include "renderer/renderer.hpp"

namespace lumina::core {

struct WindowDimensions {
  u32 width;
  u32 height;
};

struct LuminaInitializeInfo {
  platform::common::vulkan::VkInitializationResult vulkan_init_result;
  WindowDimensions window_dimensions;
};

class LuminaEngine {
public:
  static auto Instance() -> LuminaEngine & {
    auto &instance = GetStaticInstance();
    if (!instance.is_initialized) {
      throw std::runtime_error(
          "LuminaEngine not initialized, call Initialize() first");
    }
    return instance;
  }

  LuminaEngine(const LuminaEngine &) = delete;
  auto operator=(const LuminaEngine &) -> LuminaEngine & = delete;
  LuminaEngine(LuminaEngine &&) noexcept = delete;
  auto operator=(LuminaEngine &&) noexcept -> LuminaEngine & = delete;

  static auto Initialize(const LuminaInitializeInfo &init_info) -> void;
  static auto Shutdown() -> void;

  auto SetWindowDimensions(u32 width, u32 height) -> void {
    SetWindowDimensions({.width = width, .height = height});
  }
  auto SetWindowDimensions(const WindowDimensions &dimensions) -> void {
    window_dimensions = dimensions;
  }

  auto WindowResized() -> void { renderer->SetFramebufferResized(true); }

  [[nodiscard]] auto GetWindowDimensions() const -> const WindowDimensions & {
    return window_dimensions;
  }

  // This is the main frame executor
  auto ExecuteFrame() -> void;

private:
  LuminaEngine() noexcept = default;
  ~LuminaEngine() noexcept = default;

  static auto GetStaticInstance() -> LuminaEngine & {
    static auto *instance = new LuminaEngine(); // NOLINT
    return *instance;
  }

  bool is_initialized = false;

  WindowDimensions window_dimensions{};
  std::unique_ptr<job_system::JobManager> job_manager = nullptr;
  std::unique_ptr<renderer::LuminaRenderer> renderer = nullptr;
};

} // namespace lumina::core
