#pragma once

#include "lumina_types.hpp"

namespace lumina::core {

enum class VertexAttributeType : u8 {
  Position = 0,
  Normal,
  TexCoord,
  Color,
  // Application-defined channels, for per-vertex data with no rendering
  // semantics of its own — the UI's solid/text selector is the first. They
  // exist because this enum is the key ToVkAttributeDescriptions matches shader
  // inputs against, so a channel with no entry here cannot be described at all.
  //
  // Numbered rather than named after a use: two attributes in one layout must
  // have distinct types or the match picks the first, so a second such channel
  // needs Custom1 rather than a second name for the same value.
  Custom0,
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
