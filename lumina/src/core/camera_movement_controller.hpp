#pragma once

#include "common/lumina_types.hpp"
#include "entity.hpp"
#include "input/input_dispatcher.hpp"

namespace lumina::core {

class LuminaEngine;

class CameraMovementController : public IInputHandler {
public:
  CameraMovementController(EntityID camera_entity_id_) noexcept
      : camera_entity_id(camera_entity_id_) {}
  auto HandleInput(const std::span<const ActionEvent> &action_events)
      -> void override;

  auto SetControlledEntity(EntityID entity_id) -> void {
    camera_entity_id = entity_id;
  }

private:
  EntityID camera_entity_id;
  f32 move_speed = 1.0F;
  f32 look_speed = 30.0F;
};

} // namespace lumina::core
