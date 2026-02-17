#pragma once

#include "math/matrix.hpp"
#include "math/vector.hpp"

#include "trigonometry.hpp"

namespace lumina::math {
class Quaternion {
public:
  static auto FromEulerAngles(Vec3 euler_angles) -> Quaternion;

public:
  Quaternion() noexcept = default;
  Quaternion(f32 w_, f32 x_, f32 y_, f32 z_) noexcept
      : w(w_), x(x_), y(y_), z(z_) {}
  Quaternion(const Quaternion &other) noexcept = default;
  ~Quaternion() noexcept = default;
  auto operator=(const Quaternion &other) noexcept -> Quaternion & = default;
  Quaternion(Quaternion &&other) noexcept = default;
  auto operator=(Quaternion &&other) noexcept -> Quaternion & = default;

  [[nodiscard]] auto CreateRotationMatrix() const -> Mat3;
  [[nodiscard]] auto Normalize() -> Quaternion &;

private:
  f32 w{0.0F};
  f32 x{0.0F};
  f32 y{0.0F};
  f32 z{0.0F};
};

inline auto Quaternion::FromEulerAngles(Vec3 euler_angles) -> Quaternion {
  euler_angles.yaw = DegreesToRadians(euler_angles.yaw);
  euler_angles.pitch = DegreesToRadians(euler_angles.pitch);
  euler_angles.roll = DegreesToRadians(euler_angles.roll);

  f32 cy = Cos(euler_angles.yaw * 0.5F);
  f32 sy = Sin(euler_angles.yaw * 0.5F);
  f32 cp = Cos(euler_angles.pitch * 0.5F);
  f32 sp = Sin(euler_angles.pitch * 0.5F);
  f32 cr = Cos(euler_angles.roll * 0.5F);
  f32 sr = Sin(euler_angles.roll * 0.5F);

  Quaternion result;
  result.w = (cy * cp * cr) + (sy * sp * sr);
  result.x = (cy * sp * cr) + (sy * cp * sr);
  result.y = (sy * cp * cr) - (cy * sp * sr);
  result.z = (cy * cp * sr) - (sy * sp * cr);

  return result;
}

inline auto Quaternion::CreateRotationMatrix() const -> Mat3 {
  Mat3 result;

  result[0].x = 1 - (2 * (y * y + z * z));
  result[0].y = 2 * (x * y + w * z);
  result[0].z = 2 * (x * z - w * y);

  result[1].x = 2 * (x * y - w * z);
  result[1].y = 1 - (2 * (x * x + z * z));
  result[1].z = 2 * (y * z + w * x);

  result[2].x = 2 * (x * z + w * y);
  result[2].y = 2 * (y * z - w * x);
  result[2].z = 1 - (2 * (x * x + y * y));

  return result;
}

inline auto Quaternion::Normalize() -> Quaternion & {
  auto magnitude = sqrtf((w * w) + (x * x) + (y * y) + (z * z));
  w /= magnitude;
  x /= magnitude;
  y /= magnitude;
  z /= magnitude;
  return *this;
}

} // namespace lumina::math
