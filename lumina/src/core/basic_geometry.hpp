#pragma once

#include "math/vector.hpp"
#include "static_mesh.hpp"

namespace lumina::core {

class BasicGeometry {
public:
  static constexpr auto Quad() -> StaticMesh {
    StaticMesh mesh;
    std::vector<math::Vec3> positions = {
        {-0.5, -0.5, 0.0},
        {0.5, -0.5, 0.0},
        {0.5, 0.5, 0.0},
        {-0.5, 0.5, 0.0},
    };
    std::vector<math::Vec3> colors = {
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0},
        {1.0, 1.0, 1.0},
    };
    std::vector<math::Vec2> tex_coords = {
        {0.0, 1.0},
        {1.0, 1.0},
        {1.0, 0.0},
        {0.0, 0.0},
    };
    mesh.vertex_count = positions.size();
    mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = VertexAttributeType::Position,
                        .element_type = ElementType::Vec3},
        std::vector<u8>(
            reinterpret_cast<u8 *>(positions.data()),
            reinterpret_cast<u8 *>(positions.data() + positions.size())));
    mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = VertexAttributeType::Color,
                        .element_type = ElementType::Vec3},
        std::vector<u8>(reinterpret_cast<u8 *>(colors.data()),
                        reinterpret_cast<u8 *>(colors.data() + colors.size())));
    mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = VertexAttributeType::TexCoord,
                        .element_type = ElementType::Vec2},
        std::vector<u8>(
            reinterpret_cast<u8 *>(tex_coords.data()),
            reinterpret_cast<u8 *>(tex_coords.data() + tex_coords.size())));
    return mesh;
  }
};

} // namespace lumina::core
