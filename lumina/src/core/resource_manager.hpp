#pragma once

#include "lumina_types.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace lumina::core {

using ResourceHandleIndexType = u32;

constexpr ResourceHandleIndexType INVALID_RESOURCE_HANDLE_INDEX =
    static_cast<ResourceHandleIndexType>(-1);

template <typename T>
struct ResourceHandle {
  ResourceHandleIndexType index = INVALID_RESOURCE_HANDLE_INDEX;
  u32 generation = 0;

  auto operator==(const ResourceHandle &other) const -> bool = default;
};

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
  auto Get(ResourceHandle<T> handle) -> std::optional<T>;
  auto Set(ResourceHandle<T> handle, T &&resource) -> void;

private:
  struct ResourceEntry {
    u32 generation;
    T resource;
  };

  ResourceEntry resources[1024];
  std::vector<ResourceHandleIndexType> free_indices;
  ResourceHandleIndexType next_index = 0;
};

template <typename T>
auto ResourceManager<T>::Create() -> ResourceHandle<T> {
  ResourceHandleIndexType index = 0;
  if (!free_indices.empty()) {
    index = free_indices.back();
    free_indices.pop_back();
  } else {
    index = next_index++;
  }
  return ResourceHandle<T>{index, resources[index].generation};
}

template <typename T>
auto ResourceManager<T>::Destroy(ResourceHandle<T> handle) -> void {
  ASSERT(handle.index < next_index &&
             handle.index != INVALID_RESOURCE_HANDLE_INDEX,
         "Invalid resource handle");
  if (resources[handle.index].generation < handle.generation) {
    LOG_WARN(
        "Attempting to destroy resource with older generation, this means the "
        "resource referenced by the handle has been destroyed and recreated, "
        "this may indicate a bug (but not necessarily)");
    return;
  }
  resources[handle.index].generation++;
  free_indices.push_back(handle.index);
}

template <typename T>
auto ResourceManager<T>::Get(ResourceHandle<T> handle) -> std::optional<T> {
  if (handle.index >= next_index ||
      resources[handle.index].generation != handle.generation) {
    return std::nullopt;
  }
  return resources[handle.index].resource;
}

template <typename T>
auto ResourceManager<T>::Set(ResourceHandle<T> handle, T &&resource) -> void {
  if (handle.index >= next_index ||
      resources[handle.index].generation != handle.generation) {
    return;
  }
  resources[handle.index].resource = std::move(resource);
}

} // namespace lumina::core
