#pragma once

#include "lumina_types.hpp"

#include "common/data_structures/lock_free_concurrent_queue.hpp"
#include "common/lumina_assert.hpp"
#include "resource_manager_handle.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <variant>

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
  auto Create(const T &&initial_value) -> ResourceHandle<T>;

  auto Destroy(ResourceHandle<T> handle) -> void;
  auto Destroy(ResourceHandle<T> handle,
               std::function<void(ResourceHandle<T> handle, const T &resource)>
                   on_destroy) -> void;

  auto DestroyAll() -> void;
  auto
  DestroyAll(std::function<void(ResourceHandle<T> handle, const T &resource)>
                 on_destroy) -> void;

  auto Get(ResourceHandle<T> handle, bool include_inactive = false)
      -> std::optional<const T *>;

  auto Update(ResourceHandle<T> handle, const T &resource) -> void;
  auto Update(ResourceHandle<T> handle, std::function<void(T &)> patch_func)
      -> void;

  auto ForEach(std::function<void(ResourceHandle<T> handle, T &resource)> func)
      -> void;

  auto ProcessDeferredOperations() -> void;

private:
  struct ResourceEntry {
    u32 generation{};
    bool is_active = false;
    T resource{};
  };

  struct DestroyContext {
    ResourceHandle<T> handle;
    std::function<void(ResourceHandle<T> handle, const T &resource)> on_destroy;
  };

  using UpdateData = std::variant<T, std::function<void(T &)>>;
  struct UpdateContext {
    ResourceHandle<T> handle;
    UpdateData update_data;
  };

  [[nodiscard]] auto IsInFreeList(ResourceHandleIndexType index) const -> bool;

  auto DestroyInternal(
      ResourceHandle<T> handle,
      std::function<void(ResourceHandle<T> handle, const T &resource)>
          on_destroy) -> void;
  auto UpdateInternal(ResourceHandle<T> handle, UpdateData &update_data)
      -> void;

  ResourceEntry resources[1024];
  std::vector<ResourceHandleIndexType> free_indices;
  std::atomic<ResourceHandleIndexType> next_index{0};

  // deffered destruction queue
  common::data_structures::LockFreeConcurrentQueue<DestroyContext>
      destroy_queue{1024};
  // deffered update queue
  common::data_structures::LockFreeConcurrentQueue<UpdateContext> update_queue{
      1024};

  // deferred creation queue
  common::data_structures::LockFreeConcurrentQueue<ResourceHandle<T>>
      create_queue{1024};
};

template <typename T>
auto ResourceManager<T>::IsInFreeList(ResourceHandleIndexType index) const
    -> bool {
  return std::ranges::find(free_indices, index) != free_indices.end();
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
  ResourceHandle<T> handle{index, resources[index].generation};
  create_queue.Push(handle);
  return handle;
}

template <typename T>
auto ResourceManager<T>::Create(const T &&initial_value) -> ResourceHandle<T> {
  ResourceHandle<T> handle = Create();
  resources[handle.index].resource = std::move(initial_value);
  return handle;
}

template <typename T>
auto ResourceManager<T>::Destroy(ResourceHandle<T> handle) -> void {
  ASSERT(handle.index < next_index.load(std::memory_order_relaxed) &&
             handle.index != INVALID_RESOURCE_HANDLE_INDEX,
         "Invalid resource handle");
  destroy_queue.Push({handle, nullptr});
}

template <typename T>
auto ResourceManager<T>::Destroy(
    ResourceHandle<T> handle,
    std::function<void(ResourceHandle<T> handle, const T &resource)> on_destroy)
    -> void {
  ASSERT(handle.index < next_index.load(std::memory_order_relaxed) &&
             handle.index != INVALID_RESOURCE_HANDLE_INDEX,
         "Invalid resource handle");
  destroy_queue.Push({handle, on_destroy});
}

template <typename T>
auto ResourceManager<T>::DestroyInternal(
    ResourceHandle<T> handle,
    std::function<void(ResourceHandle<T> handle, const T &resource)> on_destroy)
    -> void {
  ASSERT(handle.index < next_index.load(std::memory_order_relaxed) &&
             handle.index != INVALID_RESOURCE_HANDLE_INDEX,
         "Invalid resource handle");
  if (resources[handle.index].generation != handle.generation) {
    return;
  }
  if (on_destroy) {
    on_destroy(handle, resources[handle.index].resource);
  }
  resources[handle.index].generation++;
  resources[handle.index].is_active = false;
  free_indices.push_back(handle.index);
}

template <typename T>
auto ResourceManager<T>::DestroyAll() -> void {
  DestroyAll(nullptr);
}

template <typename T>
auto ResourceManager<T>::DestroyAll(
    std::function<void(ResourceHandle<T> handle, const T &resource)> on_destroy)
    -> void {
  ForEach([this, on_destroy](ResourceHandle<T> handle, T &resource) -> void {
    Destroy(handle, on_destroy);
  });
}

template <typename T>
auto ResourceManager<T>::Get(ResourceHandle<T> handle, bool include_inactive)
    -> std::optional<const T *> {
  if (handle.index >= next_index.load(std::memory_order_relaxed) ||
      resources[handle.index].generation != handle.generation ||
      (!include_inactive && !resources[handle.index].is_active)) {
    return std::nullopt;
  }
  return &resources[handle.index].resource;
}

template <typename T>
auto ResourceManager<T>::Update(ResourceHandle<T> handle, const T &resource)
    -> void {
  update_queue.Push({handle, resource});
}

template <typename T>
auto ResourceManager<T>::Update(ResourceHandle<T> handle,
                                std::function<void(T &)> patch_func) -> void {
  update_queue.Push({handle, patch_func});
}

template <typename T>
auto ResourceManager<T>::UpdateInternal(ResourceHandle<T> handle,
                                        UpdateData &update_data) -> void {
  // check if the entry has been destroyed
  if (handle.index >= next_index.load(std::memory_order_relaxed) ||
      resources[handle.index].generation != handle.generation) {
    return;
  }

  if (std::holds_alternative<T>(update_data)) {
    resources[handle.index].resource = std::get<T>(update_data);
  } else {
    std::get<std::function<void(T &)>>(update_data)(
        resources[handle.index].resource);
  }
}

template <typename T>
auto ResourceManager<T>::ForEach(
    std::function<void(ResourceHandle<T> handle, T &resource)> func) -> void {
  for (ResourceHandleIndexType i = 0;
       i < next_index.load(std::memory_order_relaxed); ++i) {
    if (!IsInFreeList(i) && resources[i].is_active) {
      ResourceHandle<T> handle{i, resources[i].generation};
      func(handle, resources[i].resource);
    }
  }
}

template <typename T>
auto ResourceManager<T>::ProcessDeferredOperations() -> void {
  ResourceHandle<T> create_handle{};
  while (create_queue.Pop(create_handle)) {
    if (resources[create_handle.index].generation == create_handle.generation) {
      resources[create_handle.index].is_active = true;
    }
  }

  DestroyContext context;
  while (destroy_queue.Pop(context)) {
    DestroyInternal(context.handle, context.on_destroy);
  }

  UpdateContext update_context;
  while (update_queue.Pop(update_context)) {
    UpdateInternal(update_context.handle, update_context.update_data);
  }
}

} // namespace lumina::core
