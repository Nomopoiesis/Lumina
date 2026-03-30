#include "bounding_box.hpp"
#include "lumina_assert.hpp"

#include "math/linear_algebra.hpp"
#include "math/vector.hpp"

namespace lumina::core {

auto ComputeAABoundingBox(const math::Vec3 *positions, size_t vertex_count)
    -> AABoudingBox {
  ASSERT(vertex_count > 0,
         "Cannot compute bounding box for empty vertex array");

  math::Vec3 min = positions[0];
  math::Vec3 max = positions[0];

  for (size_t i = 1; i < vertex_count; ++i) {
    const math::Vec3 &pos = positions[i];
    min.x = std::min(min.x, pos.x);
    min.y = std::min(min.y, pos.y);
    min.z = std::min(min.z, pos.z);
    max.x = std::max(max.x, pos.x);
    max.y = std::max(max.y, pos.y);
    max.z = std::max(max.z, pos.z);
  }

  return {.min = min, .max = max};
}

auto TransformAABoundingBox(const AABoudingBox &aabb,
                            const math::Mat4 &transform) -> AABoudingBox {
  // Transform the 8 corners of the AABB and compute a new AABB that contains
  // them
  math::Vec3 corners[8] = {
      {aabb.min.x, aabb.min.y, aabb.min.z},
      {aabb.max.x, aabb.min.y, aabb.min.z},
      {aabb.min.x, aabb.max.y, aabb.min.z},
      {aabb.max.x, aabb.max.y, aabb.min.z},
      {aabb.min.x, aabb.min.y, aabb.max.z},
      {aabb.max.x, aabb.min.y, aabb.max.z},
      {aabb.min.x, aabb.max.y, aabb.max.z},
      {aabb.max.x, aabb.max.y, aabb.max.z},
  };
  for (math::Vec3 &corner : corners) {
    auto transformed = math::Dot(math::Vec4(corner, 1.0F), transform);
    corner = transformed.xyz();
  }
  return ComputeAABoundingBox(corners, 8);
}

auto TransformOrientedBoundingBox(const OrientedBoundingBox &obb,
                                  const math::Mat4 &transform)
    -> OrientedBoundingBox {
  OrientedBoundingBox result;
  u32 idx = 0;
  for (const math::Vec3 &corner : obb.corners) {
    auto transformed = math::Dot(math::Vec4(corner, 1.0F), transform);
    result.corners[idx++] = transformed.xyz();
  }
  return result;
}

} // namespace lumina::core