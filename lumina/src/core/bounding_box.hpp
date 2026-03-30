#pragma once

#include "math/matrix.hpp"
#include "math/vector.hpp"

namespace lumina::core {

struct OrientedBoundingBox {
  math::Vec3 corners[8];
};

struct AABoudingBox {
  math::Vec3 min;
  math::Vec3 max;
};

auto ComputeAABoundingBox(const math::Vec3 *positions, size_t vertex_count)
    -> AABoudingBox;

auto TransformAABoundingBox(const AABoudingBox &aabb,
                            const math::Mat4 &transform) -> AABoudingBox;

auto TransformOrientedBoundingBox(const OrientedBoundingBox &obb,
                                  const math::Mat4 &transform)
    -> OrientedBoundingBox;

} // namespace lumina::core