#include "vertex_layout.hpp"

namespace lumina::renderer {

auto VertexBufferLayout::Interleave(
    std::span<const core::VertexAttributeType> attributes)
    -> VertexBufferLayout {
  VertexBufferLayout layout;
  std::vector<core::VertexAttributeType> interleaved_attributes(
      attributes.begin(), attributes.end());
  layout.streams.emplace_back(VertexStreamLayout{interleaved_attributes});
  return layout;
}

auto VertexBufferLayout::Seperate(
    std::span<const core::VertexAttributeType> attributes)
    -> VertexBufferLayout {
  VertexBufferLayout layout;
  for (const auto &attribute : attributes) {
    layout.streams.emplace_back(
        VertexStreamLayout{std::vector<core::VertexAttributeType>{attribute}});
  }
  return layout;
}
} // namespace lumina::renderer