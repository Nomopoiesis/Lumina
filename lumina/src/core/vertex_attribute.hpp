#pragma once

#include "lumina_types.hpp"

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

} // namespace lumina::core
