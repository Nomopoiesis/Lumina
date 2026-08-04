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
  }
}

} // namespace lumina::core