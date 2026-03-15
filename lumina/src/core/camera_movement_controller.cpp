#include "camera_movement_controller.hpp"

#include "components/transform.hpp"
#include "engine_coordinates.hpp"
#include "lumina_engine.hpp"
#include "math/basic.hpp"
#include "math/vector.hpp"

namespace lumina::core {

auto CameraMovementController::HandleInput(
    const std::span<const ActionEvent> &action_events) -> void {
  math::Vec3 move_direction{};
  math::Vec3 look_rotation{};
  bool trap_cursor = false;
  auto &engine = LuminaEngine::Instance();
  auto dt = engine.GetFrameDeltaTimeF();
  auto &world = engine.GetCurrentWorld();
  auto camera_id = world.GetActiveCamera();
  auto transform = world.GetComponent<components::Transform>(camera_id);
  for (const auto &action_event : action_events) {
    if (action_event.action_id == ActionID(std::string_view("MoveForward"))) {
      move_direction += transform.Forward();
    }
    if (action_event.action_id == ActionID(std::string_view("MoveBackward"))) {
      move_direction -= transform.Forward();
    }
    if (action_event.action_id == ActionID(std::string_view("MoveLeft"))) {
      move_direction -= transform.Right();
    }
    if (action_event.action_id == ActionID(std::string_view("MoveRight"))) {
      move_direction += transform.Right();
    }
    if (action_event.action_id == ActionID(std::string_view("MoveUp"))) {
      move_direction += EngineCoordinates::UP;
    }
    if (action_event.action_id == ActionID(std::string_view("MoveDown"))) {
      move_direction -= EngineCoordinates::UP;
    }
    if (action_event.action_id == ActionID(std::string_view("LookVertical"))) {
      look_rotation.pitch += -action_event.axis_value;
      look_rotation.pitch = math::Clamp(look_rotation.pitch, -89.0F, 89.0F);
    }
    if (action_event.action_id ==
        ActionID(std::string_view("LookHorizontal"))) {
      look_rotation.yaw += -action_event.axis_value;
    }
    if (action_event.action_id == ActionID(std::string_view("TrapCursor"))) {
      trap_cursor = action_event.key_state == KeyState::Held;
    }
  }
  transform.Move(move_direction * move_speed * dt);
  if (trap_cursor) {
    engine.SetCursorTrapped(true);
    look_rotation *= look_speed * dt;
    transform.Rotate(look_rotation);
  } else {
    engine.SetCursorTrapped(false);
  }
  world.SetComponent<components::Transform>(camera_id, transform);
}

} // namespace lumina::core
