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
    std::vector<u16> indices = {0, 1, 2, 2, 3, 0};
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
    mesh.indices = indices;
    return mesh;
  }

  static constexpr auto Cube() -> StaticMesh {
    StaticMesh mesh;

    // 24 vertices (4 per face), UV layout matches Quad:
    //   v0=BL(0,1), v1=BR(1,1), v2=TR(1,0), v3=TL(0,0)
    // viewed from outside each face.
    std::vector<math::Vec3> positions = {
        // Front (z = +0.5)
        {-0.5, -0.5, +0.5}, {+0.5, -0.5, +0.5},
        {+0.5, +0.5, +0.5}, {-0.5, +0.5, +0.5},
        // Back (z = -0.5)
        {+0.5, -0.5, -0.5}, {-0.5, -0.5, -0.5},
        {-0.5, +0.5, -0.5}, {+0.5, +0.5, -0.5},
        // Right (x = +0.5)
        {+0.5, -0.5, +0.5}, {+0.5, -0.5, -0.5},
        {+0.5, +0.5, -0.5}, {+0.5, +0.5, +0.5},
        // Left (x = -0.5)
        {-0.5, -0.5, -0.5}, {-0.5, -0.5, +0.5},
        {-0.5, +0.5, +0.5}, {-0.5, +0.5, -0.5},
        // Top (y = +0.5)
        {-0.5, +0.5, +0.5}, {+0.5, +0.5, +0.5},
        {+0.5, +0.5, -0.5}, {-0.5, +0.5, -0.5},
        // Bottom (y = -0.5)
        {-0.5, -0.5, -0.5}, {+0.5, -0.5, -0.5},
        {+0.5, -0.5, +0.5}, {-0.5, -0.5, +0.5},
    };

    std::vector<math::Vec3> colors = {
        // Front: red
        {1.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
        {1.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
        // Back: cyan
        {0.0, 1.0, 1.0}, {0.0, 1.0, 1.0},
        {0.0, 1.0, 1.0}, {0.0, 1.0, 1.0},
        // Right: green
        {0.0, 1.0, 0.0}, {0.0, 1.0, 0.0},
        {0.0, 1.0, 0.0}, {0.0, 1.0, 0.0},
        // Left: magenta
        {1.0, 0.0, 1.0}, {1.0, 0.0, 1.0},
        {1.0, 0.0, 1.0}, {1.0, 0.0, 1.0},
        // Top: blue
        {0.0, 0.0, 1.0}, {0.0, 0.0, 1.0},
        {0.0, 0.0, 1.0}, {0.0, 0.0, 1.0},
        // Bottom: yellow
        {1.0, 1.0, 0.0}, {1.0, 1.0, 0.0},
        {1.0, 1.0, 0.0}, {1.0, 1.0, 0.0},
    };

    std::vector<math::Vec2> tex_coords = {
        {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 0.0},  // Front
        {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 0.0},  // Back
        {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 0.0},  // Right
        {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 0.0},  // Left
        {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 0.0},  // Top
        {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 0.0},  // Bottom
    };

    // 6 faces × 6 indices each = 36 indices
    std::vector<u16> indices;
    indices.reserve(36);
    for (u16 face = 0; face < 6; ++face) {
      u16 base = static_cast<u16>(face * 4);
      indices.push_back(base);
      indices.push_back(static_cast<u16>(base + 1));
      indices.push_back(static_cast<u16>(base + 2));
      indices.push_back(static_cast<u16>(base + 2));
      indices.push_back(static_cast<u16>(base + 3));
      indices.push_back(base);
    }

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
    mesh.indices = std::move(indices);
    return mesh;
  }
};

} // namespace lumina::core
