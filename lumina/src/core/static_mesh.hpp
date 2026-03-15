#pragma once

#include "lumina_types.hpp"
#include "resource_manager.hpp"

#include "math/vector.hpp"

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
  Int,
  Uint,
  Bool,
};

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

  std::vector<std::pair<VertexAttribute, std::vector<u8>>> vertex_attributes;
};

using StaticMeshHandle = ResourceHandle<StaticMesh>;

} // namespace lumina::core
