#pragma once

#include "core/vertex_attribute.hpp"

#include <span>
#include <vector>

namespace lumina::renderer {

struct VertexStreamLayout {
  std::vector<core::VertexAttribute> attributes;
};

struct VertexBufferLayout {
  std::vector<VertexStreamLayout> streams;

  static auto Interleave(std::span<const core::VertexAttribute> attributes)
      -> VertexBufferLayout;

  static auto Separate(std::span<const core::VertexAttribute> attributes)
      -> VertexBufferLayout;
};

} // namespace lumina::renderer
