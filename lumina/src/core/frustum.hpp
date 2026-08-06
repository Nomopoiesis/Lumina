#pragma once

#include "bounding_box.hpp"
#include "common/lumina_types.hpp"
#include "math/matrix.hpp"

namespace lumina::core {

inline constexpr size_t FrustumPlaneCount = 6;

// Plane order as produced by ExtractFrustumFromMatrix.
enum FrustumPlane : u8 {
  Left = 0,
  Right = 1,
  Bottom = 2,
  Top = 3,
  Near = 4,
  Far = 5,
};

// Planes stored component-wise so a test walks four contiguous arrays instead
// of striding over an array of structs.
//
// A point p is inside plane i when nx[i]*p.x + ny[i]*p.y + nz[i]*p.z + d[i] >=
// 0 and inside the frustum when that holds for all six.
class Frustum {
public:
  Frustum() noexcept = default;
  Frustum(const Frustum &) noexcept = default;
  Frustum(Frustum &&) noexcept = default;
  auto operator=(const Frustum &) noexcept -> Frustum & = default;
  auto operator=(Frustum &&) noexcept -> Frustum & = default;
  ~Frustum() noexcept = default;

  f32 nx[FrustumPlaneCount]{};
  f32 ny[FrustumPlaneCount]{};
  f32 nz[FrustumPlaneCount]{};
  f32 d[FrustumPlaneCount]{};
};

/***
 * @brief Extracts the frustum from a matrix.
 * @param matrix The matrix to extract the frustum from.
 * @return The extracted frustum, planes are not normalized.
 */
auto ExtractFrustumFromMatrix(const math::Mat4 &matrix) -> Frustum;

enum class FrustumTestResult : u8 {
  // Entirely outside at least one plane.
  Outside,
  // Not rejected, but crosses at least one plane.
  Intersecting,
  // Entirely inside all six planes. Children of a volume that tests Inside
  // need no test at all, which is what makes hierarchical culling pay.
  Inside,
};

/***
 * @brief Tests an axis-aligned box against the frustum.
 * @param frustum The frustum to test against, as produced by
 *        ExtractFrustumFromMatrix. Planes need not be normalized.
 * @param aabb Center/extent box, in the same space as the matrix the frustum
 *        was extracted from.
 * @return Outside, Intersecting, or Fully.
 *
 * Conservative in the usual way: a box lying outside two planes' half-spaces
 * without being fully outside either one reports Intersecting. That false
 * positive is the price of a six-plane test and is not worth correcting.
 */
[[nodiscard]] auto
TestAABoundingBox(const Frustum &frustum,
                  const AABoundingBoxCenterExtent &aabb) noexcept
    -> FrustumTestResult;

[[nodiscard]] auto TestAABoundingBox(const Frustum &frustum,
                                     const AABoudingBox &aabb) noexcept
    -> FrustumTestResult;

// Convenience for the common "should this be drawn" query.
[[nodiscard]] inline auto
IsAABoundingBoxVisible(const Frustum &frustum,
                       const AABoundingBoxCenterExtent &aabb) noexcept -> bool {
  return TestAABoundingBox(frustum, aabb) != FrustumTestResult::Outside;
}

} // namespace lumina::core