#pragma once

#include "lumina_types.hpp"
#include "renderer/render_mesh.hpp"
#include "resource_manager.hpp"

namespace lumina::core {

enum class VertexAttributeType : u8 {
  Position = 0,
  Normal,
  TexCoord,
  Color,
};

enum class ElementType : u8 {
  Vec2,
  Vec3,
  Vec4,
  Float,
  Double,
  Int8,
  Uint8,
  Int16,
  Uint16,
  Int32,
  Uint32,
  Int64,
  Uint64,
  Bool,
};

auto GetElementTypeSize(ElementType element_type) noexcept -> u8;

struct VertexAttribute {
  VertexAttributeType type;
  ElementType element_type;
};

class StaticMesh {
public:
  StaticMesh() noexcept = default;
  StaticMesh(const StaticMesh &other) noexcept = default;
  StaticMesh(StaticMesh &&other) noexcept = default;
  auto operator=(const StaticMesh &other) noexcept -> StaticMesh & = default;
  auto operator=(StaticMesh &&other) noexcept -> StaticMesh & = default;
  ~StaticMesh() noexcept = default;

  size_t vertex_count = 0;
  std::vector<std::pair<VertexAttribute, std::vector<u8>>> vertex_attributes;
  renderer::RenderMeshHandle render_mesh_handle;
};

using StaticMeshHandle = ResourceHandle<StaticMesh>;
using StaticMeshManager = ResourceManager<StaticMesh>;

} // namespace lumina::core
