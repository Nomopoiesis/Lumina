#pragma once

#include "bounding_box.hpp"
#include "data_structures/data_buffer.hpp"
#include "renderer/primitive_topology.hpp"
#include "renderer/render_mesh.hpp"
#include "resource_manager.hpp"
#include "vertex_attribute.hpp"

namespace lumina::core {

using common::data_structures::DataBuffer;

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
  renderer::PrimitiveTopology topology = renderer::PrimitiveTopology::TriangleList;
  std::vector<std::pair<VertexAttribute, DataBuffer>> vertex_attributes;
  std::vector<u16> indices;

  AABoudingBox bounding_box;

  renderer::RenderMeshHandle render_mesh_handle;
};

using StaticMeshHandle = ResourceHandle<StaticMesh>;
using StaticMeshManager = ResourceManager<StaticMesh>;

} // namespace lumina::core
