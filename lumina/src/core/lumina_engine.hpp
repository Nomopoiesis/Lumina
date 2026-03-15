#pragma once

#include <memory>

#include "common/logger/logger.hpp"
#include "common/lumina_terminate.hpp"
#include "platform/common/vulkan/vulkan_init_result.hpp"

#include "camera_movement_controller.hpp"
#include "input/input_dispatcher.hpp"
#include "input/input_state.hpp"
#include "job_system/job_manager.hpp"
#include "renderer/renderer.hpp"
#include "resource_manager.hpp"
#include "static_mesh.hpp"
#include "world.hpp"

namespace lumina::core {

struct WindowDimensions {
  u32 width;
  u32 height;
};

struct LuminaInitializeInfo {
  platform::common::vulkan::VkInitializationResult vulkan_init_result;
  WindowDimensions window_dimensions{};
};

struct FrameTimeInfo {
  f64 delta_time;
  f64 total_time;
};

class LuminaEngine {
public:
  static auto Instance() -> LuminaEngine & {
    auto &instance = GetStaticInstance();
    if (!instance.is_initialized) {
      LOG_CRITICAL("LuminaEngine not initialized, call Initialize() first");
      LUMINA_TERMINATE();
    }
    return instance;
  }

  LuminaEngine(const LuminaEngine &) = delete;
  auto operator=(const LuminaEngine &) -> LuminaEngine & = delete;
  LuminaEngine(LuminaEngine &&) noexcept = delete;
  auto operator=(LuminaEngine &&) noexcept -> LuminaEngine & = delete;

  [[nodiscard]] static auto IsInitialized() -> bool {
    return GetStaticInstance().is_initialized;
  }
  static auto Initialize(const LuminaInitializeInfo &init_info) -> void;
  static auto Shutdown() -> void;

  auto SetWindowDimensions(u32 width, u32 height) -> void {
    SetWindowDimensions({.width = width, .height = height});
  }
  auto SetWindowDimensions(const WindowDimensions &dimensions) -> void {
    window_dimensions = dimensions;
  }

  auto SetCursorTrapped(bool trap) -> void {
    if (trap != trap_cursor) {
      trap_cursor = trap;
      platform::common::PlatformServices::Instance().LuminaSetCursorTrapped(
          trap);
    }
  }
  [[nodiscard]] auto IsCursorTrapped() const -> bool { return trap_cursor; }

  auto WindowResized() -> void { renderer->SetFramebufferResized(true); }

  [[nodiscard]] auto GetWindowDimensions() const -> const WindowDimensions & {
    return window_dimensions;
  }

  [[nodiscard]] auto GetFrameTimeInfo() const -> const FrameTimeInfo & {
    return frame_time_info;
  }
  [[nodiscard]] auto GetFrameDeltaTime() const -> f64 {
    return frame_time_info.delta_time;
  }
  [[nodiscard]] auto GetFrameTotalTime() const -> f64 {
    return frame_time_info.total_time;
  }
  [[nodiscard]] auto GetFrameDeltaTimeF() const -> f32 {
    return static_cast<f32>(frame_time_info.delta_time);
  }
  [[nodiscard]] auto GetFrameTotalTimeF() const -> f32 {
    return static_cast<f32>(frame_time_info.total_time);
  }

  auto GetInputState() -> InputState & { return input_state; }
  [[nodiscard]] auto GetInputState() const -> const InputState & {
    return input_state;
  }

  [[nodiscard]] auto GetCurrentWorld() -> World & { return *current_world; }

  // This is the main frame executor
  auto ExecuteFrame(f64 delta_time) -> void;

private:
  LuminaEngine() noexcept = default;
  ~LuminaEngine() noexcept = default;

  static auto GetStaticInstance() -> LuminaEngine & {
    static auto *instance = new LuminaEngine(); // NOLINT
    return *instance;
  }

  bool is_initialized = false;

  bool trap_cursor = false;

  FrameTimeInfo frame_time_info{};

  std::unique_ptr<CameraMovementController> camera_movement_controller;

  InputState input_state;
  InputDispatcher input_dispatcher;

  WindowDimensions window_dimensions{};
  std::unique_ptr<job_system::JobManager> job_manager = nullptr;
  std::unique_ptr<renderer::LuminaRenderer> renderer = nullptr;

  std::unique_ptr<World> current_world;

  ResourceManager<StaticMesh> static_mesh_manager;
};

} // namespace lumina::core
