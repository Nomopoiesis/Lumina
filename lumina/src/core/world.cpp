#include "world.hpp"

namespace lumina::core {

auto World::CreateEntity() -> EntityID { return entity_manager.CreateEntity(); }

auto World::DestroyEntity(EntityID id) -> void {
  // Drop every component this entity owns before the id is recycled.
  for (auto &storage : component_storages) {
    if (storage != nullptr) {
      storage->Destroy(id);
    }
  }

  if (active_camera_id == id) {
    active_camera_id = INVALID_ENTITY_ID;
  }

  entity_manager.DestroyEntity(id);
}

auto World::GetEntity(EntityID id) -> std::optional<Entity> {
  return entity_manager.GetEntity(id);
}

auto World::SetActiveCamera(EntityID id) -> void { active_camera_id = id; }

auto World::GetActiveCamera() -> EntityID { return active_camera_id; }

} // namespace lumina::core
