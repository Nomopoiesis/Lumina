#pragma once

#include <array>
#include <memory>

#include "common/file_watcher/file_watcher.hpp"
#include "common/logger/logger.hpp"
#include "common/lumina_terminate.hpp"
#include "common/profiling/profiler.hpp"
#include "common/timer.hpp"
#include "debug_overlay.hpp"
#include "debug_overlay_controller.hpp"
#include "frame_stats.hpp"
#include "platform/platform_common/vulkan/vulkan_init_result.hpp"


#include "camera_movement_controller.hpp"
#include "drawable_proxy_manager.hpp"
#include "font.hpp"
#include "input/input_dispatcher.hpp"
#include "input/input_state.hpp"
#include "renderer/renderer.hpp"
#include "static_mesh.hpp"
#include "static_mesh_manager.hpp"
#include "texture.hpp"
#include "window_dimensions.hpp"
#include "world.hpp"

namespace lumina::core {
class LuminaEngine;
class UISystem;
} // namespace lumina::core

namespace lumina::core::job_system {
// Only ever held here as a pointer, so the job system's headers stay out of
// everything that includes the engine.
struct Counter;
} // namespace lumina::core::job_system

namespace lumina::core {

struct LuminaInitializeInfo {
  platform::common::vulkan::VkInitializationResult vulkan_init_result;
  WindowDimensions window_dimensions{};
};

struct FrameTimeInfo {
  f64 delta_time;
  f64 simulation_delta_time;
  f64 total_time;
};

struct PickRequestPosition {
  i32 x;
  i32 y;
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

  auto ToggleBoundingBoxView() -> void {
    show_bounding_boxes = !show_bounding_boxes;
    LOG_INFO("Toggling bounding boxes view mode ({})", show_bounding_boxes);
  }

  [[nodiscard]] auto IsBoundingBoxViewEnabled() const -> bool {
    return show_bounding_boxes;
  }

  auto WindowResized() -> void { renderer->SetFramebufferResized(true); }

  [[nodiscard]] auto GetWindowDimensions() const -> const WindowDimensions & {
    return window_dimensions;
  }

  // Ignored while an earlier pick is still unanswered. The answer takes a frame
  // or two to come back and there is one readback buffer behind it, so a click
  // arriving in that window is dropped rather than queued.
  auto RequestEntityPickAt(i32 x, i32 y) -> void {
    if (pick_in_flight) {
      return;
    }
    pick_in_flight = true;
    pick_request_position = PickRequestPosition{.x = x, .y = y};
  }

  [[nodiscard]] auto GetPickRequestPosition() const
      -> const std::optional<PickRequestPosition> & {
    return pick_request_position;
  }

  // The entity most recently resolved by a pick, or INVALID_ENTITY_ID if the
  // last pick hit nothing. Trails a click by a frame or two: the pick result
  // comes back from the GPU asynchronously.
  [[nodiscard]] auto GetPickedEntity() const -> EntityID {
    return picked_entity_id;
  }

  [[nodiscard]] auto GetFrameTimeInfo() const -> const FrameTimeInfo & {
    return frame_time_info;
  }
  [[nodiscard]] auto GetFrameDeltaTime() const -> f64 {
    return frame_time_info.delta_time;
  }
  [[nodiscard]] auto GetFrameSimulationDeltaTime() const -> f64 {
    return frame_time_info.simulation_delta_time;
  }
  [[nodiscard]] auto GetFrameTotalTime() const -> f64 {
    return frame_time_info.total_time;
  }
  [[nodiscard]] auto GetFrameDeltaTimeF() const -> f32 {
    return static_cast<f32>(frame_time_info.delta_time);
  }
  [[nodiscard]] auto GetFrameSimulationDeltaTimeF() const -> f32 {
    return static_cast<f32>(frame_time_info.simulation_delta_time);
  }
  [[nodiscard]] auto GetFrameTotalTimeF() const -> f32 {
    return static_cast<f32>(frame_time_info.total_time);
  }

  [[nodiscard]] auto GetRenderer() -> renderer::LuminaRenderer & {
    return *renderer;
  }

  auto GetInputState() -> InputState & { return input_state; }
  [[nodiscard]] auto GetInputState() const -> const InputState & {
    return input_state;
  }

  [[nodiscard]] auto GetCurrentWorld() -> World & { return *current_world; }

  [[nodiscard]] auto GetFrameStats() const -> const FrameStats & {
    return frame_stats;
  }

  [[nodiscard]] auto GetDebugOverlay() -> DebugOverlay & {
    return debug_overlay;
  }

  // Prepares the engine state for the next frame
  auto BeginFrame(Timer &timer) -> void;
  // This is the main frame executor
  auto ExecuteFrame() -> void;
  // Ends engine frame simulation and releases the frame for rendering
  auto EndFrame() -> void;

private:
  LuminaEngine() noexcept;
  ~LuminaEngine() noexcept;

  static auto GetStaticInstance() -> LuminaEngine &;

  auto ProcessDeferredOperations() -> void;

  bool is_initialized = false;

  bool trap_cursor = false;

  bool show_bounding_boxes = false;

  FrameTimeInfo frame_time_info{};
  FrameStats frame_stats{};
  DebugOverlay debug_overlay{};

  common::FileWatcher file_watcher;

  // Last frame's zone snapshot, plus an exponential moving average per zone.
  // Raw per-frame zone times swing too much to read at the overlay's refresh
  // rate; the EMA is indexed by zone id, which is why EndFrame writes samples
  // at their id rather than packed.
  profiling::FrameProfile frame_profile{};
  std::array<f64, profiling::MaxZones> zone_seconds_ema{};

  std::unique_ptr<CameraMovementController> camera_movement_controller;
  std::unique_ptr<DebugOverlayController> debug_overlay_controller;

  InputState input_state;
  InputDispatcher input_dispatcher;

  WindowDimensions window_dimensions{};
  std::unique_ptr<renderer::LuminaRenderer> renderer = nullptr;

  std::unique_ptr<World> current_world;

  StaticMeshManager static_mesh_manager;
  TextureManager texture_manager;

  // Flat rendering-side view of current_world, rebuilt by Sync() each frame and
  // consumed by the cull. Single-buffered on purpose: only one frame context is
  // in update at a time, so the cull's inputs never need per-frame copies.
  DrawableProxyManager drawable_proxy_manager;

  // Cull output for the current frame. Persistent so the per-chunk buffers are
  // sized once rather than reallocated every frame; single-buffered because
  // only one frame context is in update at a time.
  BatchedVisibilityIndex visibility_index;

  // Counting-sort working set for the draw list build, held here so the two
  // per-item arrays are allocated once instead of on every frame.
  DrawListScratch draw_list_scratch;

  std::unordered_map<std::string, Font> fonts;
  std::unique_ptr<UISystem> ui_system;
  StaticMeshHandle debug_aabb_mesh_handle;

  // entity picking
  auto IssuePickRequest() -> void;
  auto PollPickResult() -> void;

  // The one place a pick slot becomes a selection, and the one place the
  // in-flight flag clears. Both outcomes route through it: a slot read back
  // from the GPU, and a click that found no candidates at all.
  auto ResolvePick(u32 slot) -> void;

  std::optional<PickRequestPosition> pick_request_position;
  EntityID picked_entity_id = INVALID_ENTITY_ID;

  // Update-thread only, so a plain bool: set when a request is accepted,
  // cleared when its answer lands. It guards the renderer's single readback
  // buffer from a second concurrent pick, which is why the renderer must report
  // a result even on the paths where it abandons one — see TakePickResult.
  bool pick_in_flight = false;

  // The entity behind each slot written into the pick target, indexed by
  // slot - 1. Held until the result comes back so a hit resolves through the
  // request that produced it: looking the index up in the EntityManager one or
  // two frames later would select whoever inherited the slot if the original
  // entity died in between.
  std::vector<EntityID> pick_candidates;
  u32 last_pick_sequence = 0;

  // Opened by the draw-list job once Sync and the main cull have both finished
  // — the pick narrows the visible set they produce, and Sync reallocates the
  // proxy arrays out from under any job that reads them early. Everything after
  // that point in the job only reads, so the pick still overlaps the draw-list
  // build rather than trailing the whole frame.
  //
  // Non-null only on frames that actually issue a pick: the draw-list job
  // signals it if set, and the pick job releases it after waking.
  job_system::Counter *pick_sync_gate = nullptr;

  // A pick frustum is a needle, so this stays tiny — but a click down a long
  // corridor of 50k entities could still find a great many. The GPU resolves
  // them correctly either way; the cap is only there to keep the pass bounded.
  static constexpr size_t MAX_PICK_CANDIDATES = 256;
};

} // namespace lumina::core
