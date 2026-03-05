#pragma once

#include "static_mesh.hpp"

namespace lumina::core::components {

class StaticMeshComponent {
public:
  StaticMeshComponent() noexcept = default;
  StaticMeshComponent(const StaticMeshHandle &static_mesh_handle_) noexcept
      : static_mesh_handle(static_mesh_handle_) {}
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

private:
  StaticMeshHandle static_mesh_handle;
};

} // namespace lumina::core::components
