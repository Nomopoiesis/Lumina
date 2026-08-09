#pragma once

#include "entity.hpp"
#include "lumina_types.hpp"

#include <optional>
#include <vector>

namespace lumina::core {

class EntityManager {
public:
  EntityManager() = default;
  EntityManager(const EntityManager &) = delete;
  auto operator=(const EntityManager &) -> EntityManager & = delete;
  EntityManager(EntityManager &&) noexcept = delete;
  auto operator=(EntityManager &&) noexcept -> EntityManager & = delete;
  ~EntityManager() = default;

  auto CreateEntity(Mobility mobility) -> EntityID;
  auto DestroyEntity(EntityID id) -> void;
  auto GetEntity(EntityID id) -> std::optional<Entity>;

private:
  struct GenerationalEntry {
    u32 generation = 0;
    Entity entity;
  };

  // Grows on demand - entities.size() always tracks next_index.
  std::vector<GenerationalEntry> entities;
  std::vector<EntityIndexType> free_indices;
  EntityIndexType next_index = 0;
};

} // namespace lumina::core
