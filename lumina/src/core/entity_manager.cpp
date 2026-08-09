#include "entity_manager.hpp"

namespace lumina::core {

auto EntityManager::CreateEntity(Mobility mobility) -> EntityID {
  EntityID id{};
  if (free_indices.empty()) {
    id.index = next_index++;
    entities.emplace_back(0, Entity(mobility));
  } else {
    id.index = free_indices.back();
    free_indices.pop_back();
    entities[id.index].entity = Entity(mobility);
  }
  entities[id.index].generation++;
  id.generation = entities[id.index].generation;
  return id;
}

auto EntityManager::DestroyEntity(EntityID id) -> void {
  if (id.index >= entities.size() ||
      entities[id.index].generation != id.generation) {
    return;
  }
  entities[id.index].generation++;
  free_indices.push_back(id.index);
}

auto EntityManager::GetEntity(EntityID id) -> std::optional<Entity> {
  if (id.index >= next_index ||
      entities[id.index].generation != id.generation) {
    return std::nullopt;
  }
  return entities[id.index].entity;
}

} // namespace lumina::core
