#pragma once

#include "entity.hpp"

#include <unordered_map>

namespace lumina::core::components {

template <typename T>
class ComponentStorage {
public:
  ComponentStorage() = default;
  ComponentStorage(const ComponentStorage &other) = delete;
  ComponentStorage(ComponentStorage &&other) noexcept = delete;
  auto operator=(const ComponentStorage &other) -> ComponentStorage & = delete;
  auto operator=(ComponentStorage &&other) noexcept
      -> ComponentStorage & = delete;
  ~ComponentStorage() = default;

  template <typename... Args>
  auto Create(EntityID id, Args &&...args) -> void;

  auto Get(EntityID id) -> T;
  auto Set(EntityID id, const T &component) -> void;

  template <typename Func>
  auto ForEach(Func &&func) -> void;

private:
  std::unordered_map<EntityID, T> data_;
};

template <typename T>
template <typename... Args>
auto ComponentStorage<T>::Create(EntityID id, Args &&...args) -> void {
  static_assert(
      std::is_constructible_v<T, Args...> && std::is_default_constructible_v<T>,
      "T must be constructible from Args... and default constructible");
  data_[id] = T(std::forward<Args>(args)...);
}

template <typename T>
auto ComponentStorage<T>::Get(EntityID id) -> T {
  return data_[id];
}

template <typename T>
auto ComponentStorage<T>::Set(EntityID id, const T &component) -> void {
  data_[id] = component;
}

template <typename T>
template <typename Func>
auto ComponentStorage<T>::ForEach(Func &&func) -> void {
  for (auto &[id, component] : data_) {
    std::forward<Func>(func)(id, component);
  }
}

} // namespace lumina::core::components
