#include "debug_overlay_controller.hpp"

#include "lumina_engine.hpp"

namespace lumina::core {

auto DebugOverlayController::HandleInput(
    const std::span<const ActionEvent> &action_events) -> void {
  auto &engine = LuminaEngine::Instance();
  for (const auto &action_event : action_events) {
    if (action_event.action_id ==
        ActionID(std::string_view("ToggleBoundingBoxView"))) {
      engine.ToggleBoundingBoxView();
    }
    if (action_event.action_id ==
        ActionID(std::string_view("ToggleDebugOverlay"))) {
      engine.GetDebugOverlay().Toggle();
    }
    if (action_event.action_id == ActionID(std::string_view("PickEntity"))) {
      if (!engine.IsCursorTrapped()) {
        engine.RequestEntityPickAt(action_event.x, action_event.y);
      }
    }
  }
}

} // namespace lumina::core