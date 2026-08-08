#pragma once

#include "renderer/material_instance_handle.hpp"
#include "static_mesh.hpp"

namespace lumina::core::components {

class StaticMeshComponent {
public:
  StaticMeshComponent() noexcept = default;
  StaticMeshComponent(const StaticMeshHandle &static_mesh_handle_) noexcept
      : static_mesh_handle(static_mesh_handle_) {}
  StaticMeshComponent(
      const StaticMeshHandle &static_mesh_handle_,
      const renderer::MaterialInstanceHandle &material_instance_handle_) noexcept
      : static_mesh_handle(static_mesh_handle_),
        material_instance_handle(material_instance_handle_) {}
  StaticMeshComponent(const StaticMeshComponent &other) noexcept = default;
  StaticMeshComponent(StaticMeshComponent &&other) noexcept = default;
  auto operator=(const StaticMeshComponent &other) noexcept
      -> StaticMeshComponent & = default;
  auto operator=(StaticMeshComponent &&other) noexcept
      -> StaticMeshComponent & = default;
  ~StaticMeshComponent() noexcept = default;

  [[nodiscard]] auto GetStaticMeshHandle() const noexcept -> StaticMeshHandle {
    return static_mesh_handle;
  }

  // Invalid when the entity was created without one, which the draw-list build
  // reads as "use the renderer's default material".
  [[nodiscard]] auto GetMaterialInstanceHandle() const noexcept
      -> renderer::MaterialInstanceHandle {
    return material_instance_handle;
  }

private:
  StaticMeshHandle static_mesh_handle;
  renderer::MaterialInstanceHandle material_instance_handle;
};

} // namespace lumina::core::components
