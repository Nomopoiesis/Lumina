#pragma once

#include "math/linear_algebra.hpp"
#include "math/matrix.hpp"
#include "math/vector.hpp"

#include "trigonometry.hpp"

namespace lumina::math {
class Quaternion {
public:
  static auto FromEulerAngles(Vec3 euler_angles) -> Quaternion;
  static auto FromEulerAnglesDeg(Vec3 euler_angles) -> Quaternion;

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

inline auto Quaternion::FromEulerAnglesDeg(Vec3 euler_angles) -> Quaternion {
  euler_angles.yaw = DegreesToRadians(euler_angles.yaw);
  euler_angles.pitch = DegreesToRadians(euler_angles.pitch);
  euler_angles.roll = DegreesToRadians(euler_angles.roll);

  return FromEulerAngles(euler_angles);
}

inline auto Quaternion::FromEulerAngles(Vec3 euler_angles) -> Quaternion {
  f32 cy = Cos(euler_angles.yaw * 0.5F);
  f32 sy = Sin(euler_angles.yaw * 0.5F);
  f32 cp = Cos(euler_angles.pitch * 0.5F);
  f32 sp = Sin(euler_angles.pitch * 0.5F);
  f32 cr = Cos(euler_angles.roll * 0.5F);
  f32 sr = Sin(euler_angles.roll * 0.5F);

  Quaternion result;
  result.w = (cp * cy * cr) + (sp * sy * sr);
  result.x = (sp * cy * cr) - (cp * sy * sr);
  result.y = (cp * sy * cr) + (sp * cy * sr);
  result.z = (cp * cy * sr) - (sp * sy * cr);

  return result.Normalize();
}

inline auto Quaternion::CreateRotationMatrix() const -> Mat3 {
  Mat3 result;

  f32 xx = x * x;
  f32 yy = y * y;
  f32 zz = z * z;
  f32 xy = x * y;
  f32 xz = x * z;
  f32 zw = z * w;
  f32 yw = y * w;
  f32 yz = y * z;
  f32 xw = x * w;

  /*
   Note this is using Shuster's convention same as DirectX
   https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation#Alternative_conventions
  */
  result[0].x = 1 - (2 * (yy + zz));
  result[0].y = 2 * (xy + zw);
  result[0].z = 2 * (xz - yw);

  result[1].x = 2 * (xy - zw);
  result[1].y = 1 - (2 * (xx + zz));
  result[1].z = 2 * (yz + xw);

  result[2].x = 2 * (xz + yw);
  result[2].y = 2 * (yz - xw);
  result[2].z = 1 - (2 * (xx + yy));

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
