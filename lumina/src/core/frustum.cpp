#include "frustum.hpp"

#include "math/basic.hpp"

namespace lumina::core {

auto ExtractFrustumFromMatrix(const math::Mat4 &matrix) -> Frustum {
  Frustum frustum;
  auto col = [&matrix](size_t column_index) -> math::Vec4 {
    return math::Vec4{matrix[0][column_index], matrix[1][column_index],
                      matrix[2][column_index], matrix[3][column_index]};
  };
  math::Vec4 planes[FrustumPlaneCount];
  planes[FrustumPlane::Left] = col(0) + col(3);   // left plane
  planes[FrustumPlane::Right] = col(3) - col(0);  // right plane
  planes[FrustumPlane::Bottom] = col(1) + col(3); // bottom plane
  planes[FrustumPlane::Top] = col(3) - col(1);    // top plane
  planes[FrustumPlane::Near] = col(2);            // near plane
  planes[FrustumPlane::Far] = col(3) - col(2);    // far plane
  for (size_t i = 0; i < FrustumPlaneCount; ++i) {
    frustum.nx[i] = planes[i].x;
    frustum.ny[i] = planes[i].y;
    frustum.nz[i] = planes[i].z;
    frustum.d[i] = planes[i].w;
  }
  return frustum;
}

auto TestAABoundingBox(const Frustum &frustum,
                       const AABoundingBoxCenterExtent &aabb) noexcept
    -> FrustumTestResult {
  // No early-out on purpose. Six independent branch-free iterations vectorize,
  // and distinguishing Fully from Intersecting requires every plane anyway, so
  // the branch would only cost a misprediction.
  u32 outside_count = 0;
  u32 inside_count = 0;

  for (size_t i = 0; i < FrustumPlaneCount; ++i) {
    // Signed distance from the box center to the plane.
    const f32 center_distance = (frustum.nx[i] * aabb.center.x) +
                                (frustum.ny[i] * aabb.center.y) +
                                (frustum.nz[i] * aabb.center.z) + frustum.d[i];

    // How far the box reaches from its center towards the plane. Using the
    // absolute value of each normal component selects the corner facing the
    // plane without enumerating all eight.
    const f32 projected_extent = (math::Abs(frustum.nx[i]) * aabb.extent.x) +
                                 (math::Abs(frustum.ny[i]) * aabb.extent.y) +
                                 (math::Abs(frustum.nz[i]) * aabb.extent.z);

    // Unnormalized planes are fine: both terms scale linearly with the normal's
    // length, so comparing them against each other is scale-invariant. That is
    // what lets ExtractFrustumFromMatrix skip six square roots.
    outside_count +=
        static_cast<u32>(center_distance < -projected_extent); // no corner in
    inside_count +=
        static_cast<u32>(center_distance > projected_extent); // every corner in
  }

  if (outside_count != 0) {
    return FrustumTestResult::Outside;
  }
  return inside_count == FrustumPlaneCount ? FrustumTestResult::Inside
                                           : FrustumTestResult::Intersecting;
}

auto TestAABoundingBox(const Frustum &frustum,
                       const AABoudingBox &aabb) noexcept -> FrustumTestResult {
  return TestAABoundingBox(frustum, ToCenterExtent(aabb));
}

} // namespace lumina::core