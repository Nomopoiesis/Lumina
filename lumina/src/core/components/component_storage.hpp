#pragma once

#include "common/lumina_assert.hpp"
#include "core/entity.hpp"

#include <atomic>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace lumina::core::components {

template <typename T>
struct TracksAdditions : std::false_type {};

// Type-erased view of a component storage, so World can drop every component
// belonging to an entity without knowing which component types exist.
// Only the destroy path is virtual - component access goes through the
// concrete storage type and stays a direct call.
class IComponentStorage {
public:
  IComponentStorage() = default;
  IComponentStorage(const IComponentStorage &) = delete;
  IComponentStorage(IComponentStorage &&) noexcept = delete;
  auto operator=(const IComponentStorage &) -> IComponentStorage & = delete;
  auto operator=(IComponentStorage &&) noexcept -> IComponentStorage & = delete;
  virtual ~IComponentStorage() = default;

  // Removes this entity's component if present, no-op otherwise.
  // Not safe to call while iterating the same storage with ForEach.
  virtual auto Destroy(EntityID id) -> void = 0;
  [[nodiscard]] virtual auto Has(EntityID id) const -> bool = 0;
  virtual auto ClearAdditions() -> void = 0;
};

// Dense index assigned to each component type on first use, used to slot
// storages into World's registry.
[[nodiscard]] inline auto NextComponentTypeIndex() -> size_t {
  static std::atomic<size_t> next_index{0};
  return next_index.fetch_add(1, std::memory_order_relaxed);
}

template <typename T>
[[nodiscard]] auto ComponentTypeIndex() -> size_t {
  static const size_t index = NextComponentTypeIndex();
  return index;
}

template <typename T>
class ComponentStorage : public IComponentStorage {
public:
  ComponentStorage() = default;
  ComponentStorage(const ComponentStorage &other) = delete;
  ComponentStorage(ComponentStorage &&other) noexcept = delete;
  auto operator=(const ComponentStorage &other) -> ComponentStorage & = delete;
  auto operator=(ComponentStorage &&other) noexcept
      -> ComponentStorage & = delete;
  ~ComponentStorage() override = default;

  template <typename... Args>
  auto Create(EntityID id, Args &&...args) -> void;

  auto Get(EntityID id) -> T;
  auto Set(EntityID id, const T &component) -> void;

  auto Destroy(EntityID id) -> void override;
  [[nodiscard]] auto Has(EntityID id) const -> bool override;

  template <typename Func>
  auto ForEach(Func &&func) -> void;

  [[nodiscard]] auto GetAdded() const -> std::span<const EntityID> {
    static_assert(TracksAdditions<T>::value,
                  "Component does not track additions; specialise "
                  "TracksAdditions<T> to enable it");
    return added_;
  }

  auto ClearAdditions() -> void override {
    added_.clear();
    added_.swap(pending_);
  }

private:
  std::unordered_map<EntityID, T> data_;
  std::vector<EntityID> added_;
  std::vector<EntityID> pending_;
};

template <typename T>
template <typename... Args>
auto ComponentStorage<T>::Create(EntityID id, Args &&...args) -> void {
  static_assert(
      std::is_constructible_v<T, Args...> && std::is_default_constructible_v<T>,
      "T must be constructible from Args... and default constructible");
  data_[id] = T(std::forward<Args>(args)...);
  if constexpr (TracksAdditions<T>::value) {
    pending_.push_back(id);
  }
}

template <typename T>
auto ComponentStorage<T>::Get(EntityID id) -> T {
  // find rather than operator[], which would resurrect a destroyed component
  // as a default-constructed entry.
  auto entry = data_.find(id);
  ASSERT(entry != data_.end(),
         "No component of this type registered for entity");
  if (entry == data_.end()) {
    return T{};
  }
  return entry->second;
}

template <typename T>
auto ComponentStorage<T>::Set(EntityID id, const T &component) -> void {
  data_[id] = component;
}

template <typename T>
auto ComponentStorage<T>::Destroy(EntityID id) -> void {
  data_.erase(id);
}

template <typename T>
auto ComponentStorage<T>::Has(EntityID id) const -> bool {
  return data_.contains(id);
}

template <typename T>
template <typename Func>
auto ComponentStorage<T>::ForEach(Func &&func) -> void {
  for (auto &[id, component] : data_) {
    std::forward<Func>(func)(id, component);
  }
}

} // namespace lumina::core::components
