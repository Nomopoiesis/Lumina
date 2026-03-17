#pragma once

#include "vertex_attribute.hpp"
#include "renderer/render_mesh.hpp"
#include "resource_manager.hpp"

namespace lumina::core {

class StaticMesh {
public:
  StaticMesh() noexcept = default;
  StaticMesh(const StaticMesh &other) noexcept = default;
  StaticMesh(StaticMesh &&other) noexcept = default;
  auto operator=(const StaticMesh &other) noexcept -> StaticMesh & = default;
  auto operator=(StaticMesh &&other) noexcept -> StaticMesh & = default;
  ~StaticMesh() noexcept = default;

  bool render_active = false;
  size_t vertex_count = 0;
  std::vector<std::pair<VertexAttribute, std::vector<u8>>> vertex_attributes;
  std::vector<u16> indices;
  renderer::RenderMeshHandle render_mesh_handle;
};

using StaticMeshHandle = ResourceHandle<StaticMesh>;
using StaticMeshManager = ResourceManager<StaticMesh>;

} // namespace lumina::core
