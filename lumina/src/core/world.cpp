#include "world.hpp"

namespace lumina::core {

auto World::CreateEntity() -> EntityID { return entity_manager.CreateEntity(); }

auto World::DestroyEntity(EntityID id) -> void {
  entity_manager.DestroyEntity(id);
}

auto World::GetEntity(EntityID id) -> std::optional<Entity> {
  return entity_manager.GetEntity(id);
}

auto World::SetActiveCamera(EntityID id) -> void { active_camera_id = id; }

auto World::GetActiveCamera() -> EntityID { return active_camera_id; }

} // namespace lumina::core
