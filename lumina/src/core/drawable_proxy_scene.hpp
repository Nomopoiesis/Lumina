#pragma once

#include "lumina_types.hpp"
#include "math/matrix.hpp"
#include "renderer/material_instance_handle.hpp"
#include "renderer/renderer.hpp"
#include "static_mesh.hpp"
#include "world.hpp"

#include <vector>

namespace lumina::core {

// Rendering-side representation of the scene: a flat SoA snapshot of everything
// drawable, rebuilt from the ECS once per frame.
//
// This lives in core rather than the renderer because every consumer is
// core-side — Sync walks the World, and the cull and draw-list passes read
// these arrays. The renderer only ever sees the resulting draw commands, and
// keeping it that way is what stops it from having to know what an ECS is.
class DrawableProxyScene {
public:
  DrawableProxyScene() noexcept = default;
  ~DrawableProxyScene() noexcept = default;

  DrawableProxyScene(const DrawableProxyScene &) noexcept = delete;
  auto operator=(const DrawableProxyScene &) noexcept
      -> DrawableProxyScene & = delete;

  DrawableProxyScene(DrawableProxyScene &&) noexcept = delete;
  auto operator=(DrawableProxyScene &&) noexcept
      -> DrawableProxyScene & = delete;

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
