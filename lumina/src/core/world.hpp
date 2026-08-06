#pragma once

#include "components/component_storage.hpp"
#include "entity_manager.hpp"

#include "components/transform.hpp"

#include <memory>
#include <vector>

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

  template <typename T>
  auto RemoveComponent(EntityID id) -> void;

  template <typename T>
  [[nodiscard]] auto HasComponent(EntityID id) -> bool;

  template <typename T, typename Func>
  auto ForEachComponent(Func &&func) -> void;

  auto SetActiveCamera(EntityID id) -> void;

  [[nodiscard]] auto GetActiveCamera() -> EntityID;

  [[nodiscard]] auto GetTransform(EntityID id) -> components::Transform {
    return GetComponent<components::Transform>(id);
  }

  [[nodiscard]] auto GetPosition(EntityID id) -> math::Vec3 {
    return GetTransform(id).position;
  }

private:
  EntityManager entity_manager;
  EntityID active_camera_id = INVALID_ENTITY_ID;

  // Indexed by components::ComponentTypeIndex<T>(), populated on first use of
  // each component type. Owned per world, so component data dies with the
  // world that holds it.
  std::vector<std::unique_ptr<components::IComponentStorage>>
      component_storages;

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
auto World::RemoveComponent(EntityID id) -> void {
  GetComponentStorage<T>()->Destroy(id);
}

template <typename T>
auto World::HasComponent(EntityID id) -> bool {
  return GetComponentStorage<T>()->Has(id);
}

template <typename T>
auto World::GetComponentStorage() -> components::ComponentStorage<T> * {
  const auto type_index = components::ComponentTypeIndex<T>();
  if (type_index >= component_storages.size()) {
    component_storages.resize(type_index + 1);
  }
  if (component_storages[type_index] == nullptr) {
    component_storages[type_index] =
        std::make_unique<components::ComponentStorage<T>>();
  }
  return static_cast<components::ComponentStorage<T> *>(
      component_storages[type_index].get());
}

template <typename T, typename Func>
auto World::ForEachComponent(Func &&func) -> void {
  GetComponentStorage<T>()->ForEach(std::forward<Func>(func));
}

} // namespace lumina::core
