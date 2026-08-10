#pragma once

#include "lumina_types.hpp"
#include "math/matrix.hpp"
#include "renderer/material_instance_handle.hpp"
#include "renderer/renderer.hpp"
#include "static_mesh_registry.hpp"
#include "world.hpp"

#include <vector>

namespace lumina::core {

class BatchedVisibilityIndex {
public:
  BatchedVisibilityIndex() noexcept = default;
  BatchedVisibilityIndex(size_t proxy_count, size_t batch_size) noexcept
      : batch_size_{batch_size}, chunked_visibility_index(proxy_count),
        chunked_visibility_index_batch_size((proxy_count + batch_size - 1) /
                                            batch_size) {}
  BatchedVisibilityIndex(const BatchedVisibilityIndex &) noexcept = delete;
  auto operator=(const BatchedVisibilityIndex &) noexcept
      -> BatchedVisibilityIndex & = delete;
  BatchedVisibilityIndex(BatchedVisibilityIndex &&) noexcept = delete;
  auto operator=(BatchedVisibilityIndex &&) noexcept
      -> BatchedVisibilityIndex & = delete;
  ~BatchedVisibilityIndex() noexcept = default;

  // Sizes the index for a cull over proxy_count proxies. resize() is a no-op
  // once the counts settle, so reusing one instance across frames keeps the
  // steady state allocation-free.
  auto Reset(size_t proxy_count, size_t batch_size) -> void {
    chunked_visibility_index.resize(proxy_count);
    chunked_visibility_index_batch_size.resize((proxy_count + batch_size - 1) /
                                               batch_size);
    batch_size_ = batch_size;
    visible_count = 0;
  }

  auto SetVisibleIndex(size_t index, size_t visible_index) -> void {
    chunked_visibility_index[index] = visible_index;
    visible_count++;
  }
  auto SetChunkSize(size_t chunk_index, size_t batch_size) -> void {
    chunked_visibility_index_batch_size[chunk_index] = batch_size;
  }
  [[nodiscard]] auto GetVisibleIndex(size_t index) const -> size_t {
    return chunked_visibility_index[index];
  }
  [[nodiscard]] auto GetChunkSize(size_t chunk_index) const -> size_t {
    return chunked_visibility_index_batch_size[chunk_index];
  }
  [[nodiscard]] auto GetChunkCount() const -> size_t {
    return chunked_visibility_index_batch_size.size();
  }
  [[nodiscard]] auto GetVisibleCount() const -> size_t { return visible_count; }

  template <typename F>
  auto ForEachVisible(F &&func) const -> void {
    for (size_t chunk = 0; chunk < GetChunkCount(); ++chunk) {
      const size_t base = chunk * batch_size_;
      const size_t visible = GetChunkSize(chunk);
      for (size_t n = 0; n < visible; ++n) {
        func(GetVisibleIndex(base + n));
      }
    }
  }

private:
  size_t batch_size_ = 0;
  std::vector<size_t> chunked_visibility_index;
  std::vector<size_t> chunked_visibility_index_batch_size;
  size_t visible_count = 0;
};

// Per-draw-item working arrays for the counting sort that groups visible
// proxies into batches. Both are indexed by draw item id and are as small as
// the registry, so keeping them alive between frames costs almost nothing and
// removes two allocations from the frame.
struct DrawListScratch {
  std::vector<u32> counts;
  // Each item's start offset before the scatter, and its write cursor during.
  std::vector<u32> offsets;
};

// Rendering-side representation of the scene: a flat SoA snapshot of everything
// drawable, rebuilt from the ECS once per frame.
//
// This lives in core rather than the renderer because every consumer is
// core-side — Sync walks the World, and the cull and draw-list passes read
// these arrays. The renderer only ever sees the resulting draw commands, and
// keeping it that way is what stops it from having to know what an ECS is.
class DrawableProxyManager {
public:
  DrawableProxyManager() noexcept = default;
  ~DrawableProxyManager() noexcept = default;

  DrawableProxyManager(const DrawableProxyManager &) noexcept = delete;
  auto operator=(const DrawableProxyManager &) noexcept
      -> DrawableProxyManager & = delete;

  DrawableProxyManager(DrawableProxyManager &&) noexcept = delete;
  auto operator=(DrawableProxyManager &&) noexcept
      -> DrawableProxyManager & = delete;

  auto Sync(World &world, StaticMeshResourceRegistry &static_mesh_registry,
            renderer::LuminaRenderer &renderer) -> void;
  [[nodiscard]] auto ProxyCount() const -> size_t { return center_x.size(); }

  [[nodiscard]] auto GetProxyAABB(size_t index) const
      -> AABoundingBoxCenterExtent {
    return {
        .center = math::Vec3{center_x[index], center_y[index], center_z[index]},
        .extent = math::Vec3{extent_x[index], extent_y[index], extent_z[index]},
    };
  }

  // Parallel arrays, indexed together. Cleared rather than freed between
  // frames, so the steady state performs no allocation.
  //
  // Bounds are world-space: the frustum planes they are tested against are, so
  // the model transform is already baked in here.
  size_t static_proxy_count = 0;
  std::vector<f32> center_x, center_y, center_z;
  std::vector<f32> extent_x, extent_y, extent_z;
  std::vector<math::Mat4> model;
  std::vector<u32> draw_item_indices;

  // The proxy -> entity direction, which nothing else records: the cull and the
  // draw-list build only ever need the index. Picking resolves a hit proxy back
  // to the entity that owns it through this.
  std::vector<EntityID> entity_ids;

  std::vector<EntityID> pending_static_entities;
  std::vector<EntityID> dynamic_entities;

private:
  auto
  ProcessPendingStaticEntities(World &world,
                               StaticMeshResourceRegistry &static_mesh_registry,
                               renderer::LuminaRenderer &renderer) -> void;

  auto ProcessRegisteredDynamicEntities(
      World &world, StaticMeshResourceRegistry &static_mesh_registry,
      renderer::LuminaRenderer &renderer) -> void;

  auto ProcessProxy(World &world,
                    StaticMeshResourceRegistry &static_mesh_registry,
                    renderer::LuminaRenderer &renderer, EntityID entity_id)
      -> bool;
};

} // namespace lumina::core
