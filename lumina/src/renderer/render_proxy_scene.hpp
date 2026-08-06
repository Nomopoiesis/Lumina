#pragma once
#include "core/static_mesh.hpp"
#include "core/world.hpp"
#include "lumina_types.hpp"
#include "math/matrix.hpp"
#include "renderer/material_instance_handle.hpp"
#include "renderer/renderer.hpp"
#include <vector>

namespace lumina::renderer {

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

  auto Sync(core::World &world, core::StaticMeshManager &static_mesh_manager,
            LuminaRenderer &renderer) -> void;
  [[nodiscard]] auto ProxyCount() const -> size_t { return center_x.size(); }

  // SoA, 64-byte aligned, persistent (no per-frame allocation)
  std::vector<f32> center_x, center_y, center_z;
  std::vector<f32> extent_x, extent_y, extent_z;
  std::vector<math::Mat4> model;
  std::vector<RenderMeshHandle> mesh_handle;
  std::vector<MaterialInstanceHandle> material;
};

} // namespace lumina::renderer