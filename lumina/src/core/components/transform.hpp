#pragma once

#include "core/engine_coordinates.hpp"
#include "math/quaternion.hpp"
#include "math/vector.hpp"

namespace lumina::core::components {

// Transform component, this is a "view" into the transform SoA actual layout
class Transform {
public:
  Transform() noexcept = default;
  Transform(const math::Vec3 &position, const math::Vec3 &rotation,
            const math::Vec3 &scale) noexcept
      : position(position), rotation(rotation), scale(scale) {}
  Transform(const Transform &other) noexcept = default;
  Transform(Transform &&other) noexcept = default;
  auto operator=(const Transform &other) -> Transform & = default;
  auto operator=(Transform &&other) noexcept -> Transform & = default;
  ~Transform() = default;

  [[nodiscard]] auto Forward() const -> math::Vec3;
  [[nodiscard]] auto Right() const -> math::Vec3;
  [[nodiscard]] auto LocalUp() const -> math::Vec3;

  auto Move(const math::Vec3 &delta) -> void;
  auto Rotate(const math::Vec3 &delta) -> void;
  auto Scale(const math::Vec3 &delta) -> void;

  math::Vec3 position;             // Position in world space (in centimeters)
  math::Vec3 rotation;             // Euler angles in degrees
  math::Vec3 scale{math::Vec3(1)}; // Scale in local space (1.0f is 100%)
};

inline auto Transform::Forward() const -> math::Vec3 {
  auto rotation_quat = math::Quaternion::FromEulerAnglesDeg(rotation);
  return math::Normalize(math::Dot(EngineCoordinates::FORWARD,
                                   rotation_quat.CreateRotationMatrix()));
}

inline auto Transform::Right() const -> math::Vec3 {
  auto forward = Forward();
  auto up = EngineCoordinates::UP;
  return math::Normalize(math::Cross(forward, up));
}

inline auto Transform::LocalUp() const -> math::Vec3 {
  auto forward = Forward();
  auto right = Right();
  return math::Normalize(math::Cross(right, forward));
}

inline auto Transform::Move(const math::Vec3 &delta) -> void {
  position += delta;
}

inline auto Transform::Rotate(const math::Vec3 &delta) -> void {
  rotation += delta;
}

inline auto Transform::Scale(const math::Vec3 &delta) -> void {
  scale += delta;
}

inline auto CalculateViewMatrix(const Transform &transform) -> math::Mat4 {
  auto forward = transform.Forward();
  auto right = transform.Right();
  auto local_up = transform.LocalUp();

  auto result = math::Mat4::Identity();
  result[0] = math::Vec4(right.x, local_up.x, -forward.x, 0);
  result[1] = math::Vec4(right.y, local_up.y, -forward.y, 0);
  result[2] = math::Vec4(right.z, local_up.z, -forward.z, 0);
  result[3] = math::Vec4(-math::Dot(right, transform.position),
                         -math::Dot(local_up, transform.position),
                         math::Dot(forward, transform.position), 1.0F);

  return result;
}

} // namespace lumina::core::components