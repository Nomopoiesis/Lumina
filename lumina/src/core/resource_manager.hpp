#pragma once

#include "lumina_types.hpp"

#include "common/logger/logger.hpp"
#include "common/lumina_assert.hpp"
#include "resource_manager_handle.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace lumina::core {

template <typename T>
class ResourceManager {
public:
  ResourceManager() noexcept = default;
  ResourceManager(const ResourceManager &other) noexcept = delete;
  ResourceManager(ResourceManager &&other) noexcept = delete;
  auto operator=(const ResourceManager &other) noexcept
      -> ResourceManager & = delete;
  auto operator=(ResourceManager &&other) noexcept
      -> ResourceManager & = delete;
  ~ResourceManager() noexcept = default;

  auto Create() -> ResourceHandle<T>;
  auto Destroy(ResourceHandle<T> handle) -> void;

  // Destroy with a callback invoked with the resource before it is freed.
  template <typename Func>
  auto Destroy(ResourceHandle<T> handle, Func &&on_destroy) -> void;

  // Destroy all active resources, invoking the callback for each.
  template <typename Func>
  auto DestroyAll(Func &&on_destroy) -> void;

  auto Get(ResourceHandle<T> handle) -> std::optional<T>;
  auto Set(ResourceHandle<T> handle, T &&resource) -> void;

  template <typename Func>
  auto ForEach(Func &&func) -> void;

private:
  struct ResourceEntry {
    u32 generation{};
    T resource{};
  };

  [[nodiscard]] auto IsActive(ResourceHandleIndexType index) const -> bool;

  ResourceEntry resources[1024];
  std::vector<ResourceHandleIndexType> free_indices;
  std::atomic<ResourceHandleIndexType> next_index{0};
};

template <typename T>
auto ResourceManager<T>::IsActive(ResourceHandleIndexType index) const -> bool {
  return std::ranges::none_of(free_indices,
                              [index](auto i) -> bool { return i == index; });
}

template <typename T>
auto ResourceManager<T>::Create() -> ResourceHandle<T> {
  ResourceHandleIndexType index = 0;
  if (!free_indices.empty()) {
    index = free_indices.back();
    free_indices.pop_back();
  } else {
    index = next_index.fetch_add(1);
  }
  return ResourceHandle<T>{index, resources[index].generation};
}

template <typename T>
auto ResourceManager<T>::Destroy(ResourceHandle<T> handle) -> void {
  ASSERT(handle.index < next_index.load(std::memory_order_relaxed) &&
             handle.index != INVALID_RESOURCE_HANDLE_INDEX,
         "Invalid resource handle");
  if (resources[handle.index].generation < handle.generation) {
    LOG_WARNING(
        "Attempting to destroy resource with older generation, this means the "
        "resource referenced by the handle has been destroyed and recreated, "
        "this may indicate a bug (but not necessarily)");
    return;
  }
  resources[handle.index].generation++;
  free_indices.push_back(handle.index);
}

template <typename T>
template <typename Func>
auto ResourceManager<T>::Destroy(ResourceHandle<T> handle, Func &&on_destroy)
    -> void {
  ASSERT(handle.index < next_index.load(std::memory_order_relaxed) &&
             handle.index != INVALID_RESOURCE_HANDLE_INDEX,
         "Invalid resource handle");
  if (resources[handle.index].generation < handle.generation) {
    LOG_WARNING(
        "Attempting to destroy resource with older generation, this means the "
        "resource referenced by the handle has been destroyed and recreated, "
        "this may indicate a bug (but not necessarily)");
    return;
  }
  std::forward<Func>(on_destroy)(resources[handle.index].resource);
  resources[handle.index].generation++;
  free_indices.push_back(handle.index);
}

template <typename T>
template <typename Func>
auto ResourceManager<T>::DestroyAll(Func &&on_destroy) -> void {
  std::vector<ResourceHandle<T>> active_handles;
  for (ResourceHandleIndexType i = 0;
       i < next_index.load(std::memory_order_relaxed); ++i) {
    if (IsActive(i)) {
      active_handles.push_back({i, resources[i].generation});
    }
  }
  for (auto &handle : active_handles) {
    std::forward<Func>(on_destroy)(handle);
  }
}

template <typename T>
auto ResourceManager<T>::Get(ResourceHandle<T> handle) -> std::optional<T> {
  if (handle.index >= next_index.load(std::memory_order_relaxed) ||
      resources[handle.index].generation != handle.generation) {
    return std::nullopt;
  }
  return resources[handle.index].resource;
}

template <typename T>
auto ResourceManager<T>::Set(ResourceHandle<T> handle, T &&resource) -> void {
  if (handle.index >= next_index.load(std::memory_order_relaxed) ||
      resources[handle.index].generation != handle.generation) {
    return;
  }
  resources[handle.index].resource = std::move(resource);
}

template <typename T>
template <typename Func>
auto ResourceManager<T>::ForEach(Func &&func) -> void {
  for (ResourceHandleIndexType i = 0;
       i < next_index.load(std::memory_order_relaxed); ++i) {
    if (IsActive(i)) {
      ResourceHandle<T> handle{i, resources[i].generation};
      std::forward<Func>(func)(handle, resources[i].resource);
    }
  }
}

} // namespace lumina::core
