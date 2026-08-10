#include "bounding_box.hpp"
#include "lumina_assert.hpp"

#include "math/basic.hpp"
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
  return ToMinMax(TransformToCenterExtent(aabb, transform));
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

auto ToCenterExtent(const AABoudingBox &aabb) -> AABoundingBoxCenterExtent {
  return {.center = (aabb.min + aabb.max) * 0.5F,
          .extent = (aabb.max - aabb.min) * 0.5F};
}

auto ToMinMax(const AABoundingBoxCenterExtent &aabbce) -> AABoudingBox {
  return {.min = aabbce.center - aabbce.extent,
          .max = aabbce.center + aabbce.extent};
}

auto TransformToCenterExtent(const AABoudingBox &aabb,
                             const math::Mat4 &transform)
    -> AABoundingBoxCenterExtent {
  auto aabbce = ToCenterExtent(aabb);
  math::Vec3 extent(0.0F);
  math::Vec3 center(0.0F);
  for (size_t i = 0; i < 3; ++i) {
    f32 axis_x = math::Abs(transform[0][i]);
    f32 axis_y = math::Abs(transform[1][i]);
    f32 axis_z = math::Abs(transform[2][i]);
    extent[i] = (aabbce.extent.x * axis_x) + (aabbce.extent.y * axis_y) +
                (aabbce.extent.z * axis_z);
    center[i] = (aabbce.center.x * transform[0][i]) +
                (aabbce.center.y * transform[1][i]) +
                (aabbce.center.z * transform[2][i]) + transform[3][i];
  }
  return {.center = center, .extent = extent};
}

} // namespace lumina::core
