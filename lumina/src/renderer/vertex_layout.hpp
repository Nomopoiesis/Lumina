#pragma once

#include "core/static_mesh.hpp"

#include <span>
#include <vector>

namespace lumina::renderer {

struct VertexStreamLayout {
  std::vector<core::VertexAttributeType> attributes;
};

struct VertexBufferLayout {
  std::vector<VertexStreamLayout> streams;

  static auto Interleave(std::span<const core::VertexAttributeType> attributes)
      -> VertexBufferLayout;

  static auto Seperate(std::span<const core::VertexAttributeType> attributes)
      -> VertexBufferLayout;
};

} // namespace lumina::renderer