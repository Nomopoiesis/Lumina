#pragma once

#include "components/component_storage.hpp"
#include "entity_manager.hpp"

namespace lumina::core {

class World {
public:
  World() = default;
  ~World() = default;

  auto CreateEntity() -> EntityID;
  auto DestroyEntity(EntityID id) -> void;
  auto GetEntity(EntityID id) -> std::optional<Entity>;

  template <typename T, typename... Args>
  auto AddComponent(EntityID id, Args &&...args) -> void;

  template <typename T>
  auto GetComponent(EntityID id) -> T;

  template <typename T>
  auto SetComponent(EntityID id, const T &component) -> void;

  auto SetActiveCamera(EntityID id) -> void;

  [[nodiscard]] auto GetActiveCamera() -> EntityID;

private:
  EntityManager entity_manager;
  EntityID active_camera_id = INVALID_ENTITY_ID;

  template <typename T>
  auto GetComponentStorage() -> components::ComponentStorage<T> *;
};

template <typename T, typename... Args>
auto World::AddComponent(EntityID id, Args &&...args) -> void {
  GetComponentStorage<T>()->Create(id, std::forward<Args>(args)...);
}

template <typename T>
auto World::GetComponent(EntityID id) -> T {
  return GetComponentStorage<T>()->Get(id);
}

template <typename T>
auto World::SetComponent(EntityID id, const T &component) -> void {
  GetComponentStorage<T>()->Set(id, component);
}

template <typename T>
auto World::GetComponentStorage() -> components::ComponentStorage<T> * {
  static components::ComponentStorage<T> storage;
  return &storage;
}

} // namespace lumina::core
