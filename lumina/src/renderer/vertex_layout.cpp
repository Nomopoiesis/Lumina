#include "vertex_layout.hpp"

namespace lumina::renderer {

auto VertexBufferLayout::Interleave(
    std::span<const core::VertexAttribute> attributes) -> VertexBufferLayout {
  VertexBufferLayout layout;
  layout.streams.emplace_back(
      VertexStreamLayout{std::vector<core::VertexAttribute>(attributes.begin(),
                                                            attributes.end())});
  return layout;
}

auto VertexBufferLayout::Separate(
    std::span<const core::VertexAttribute> attributes) -> VertexBufferLayout {
  VertexBufferLayout layout;
  for (const auto &attribute : attributes) {
    layout.streams.emplace_back(
        VertexStreamLayout{std::vector<core::VertexAttribute>{attribute}});
  }
  return layout;
}

} // namespace lumina::renderer
