#pragma once

#include "lumina_types.hpp"
#include "math/matrix.hpp"
#include "renderer/material_instance_handle.hpp"
#include "renderer/renderer.hpp"
#include "static_mesh.hpp"
#include "world.hpp"

#include <vector>

namespace lumina::core {

class BatchedVisibilityIndex {
public:
  BatchedVisibilityIndex() noexcept = default;
  BatchedVisibilityIndex(size_t proxy_count, size_t batch_size) noexcept
      : chunked_visibility_index(proxy_count),
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
  }

  auto SetVisibleIndex(size_t index, size_t visible_index) -> void {
    chunked_visibility_index[index] = visible_index;
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

private:
  std::vector<size_t> chunked_visibility_index;
  std::vector<size_t> chunked_visibility_index_batch_size;
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

  auto Sync(World &world, StaticMeshManager &static_mesh_manager,
            renderer::LuminaRenderer &renderer) -> void;
  [[nodiscard]] auto ProxyCount() const -> size_t { return center_x.size(); }

  // Parallel arrays, indexed together. Cleared rather than freed between
  // frames, so the steady state performs no allocation.
  //
  // Bounds are world-space: the frustum planes they are tested against are, so
  // the model transform is already baked in here.
  std::vector<f32> center_x, center_y, center_z;
  std::vector<f32> extent_x, extent_y, extent_z;

  std::vector<math::Mat4> model;
  std::vector<renderer::RenderMeshHandle> mesh_handle;
  std::vector<renderer::MaterialInstanceHandle> material;
};

} // namespace lumina::core
