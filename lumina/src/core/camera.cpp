#include "camera.hpp"

#include "math/trigonometry.hpp"

namespace lumina::core {

auto CalculatePerspectiveMatrix(f32 fov_degrees, f32 aspect_ratio,
                                f32 near_plane, f32 far_plane) -> math::Mat4 {
  f32 fov_rad = math::DegreesToRadians(fov_degrees);
  f32 tan_half_fov = math::Tan(fov_rad * 0.5F);
  f32 range = far_plane - near_plane;

  math::Mat4 result{};

  result[0][0] = 1.0F / (aspect_ratio * tan_half_fov);
  result[1][1] = 1.0F / tan_half_fov;
  result[2][2] = -(far_plane + near_plane) / range;
  result[2][3] = -1.0F;
  result[3][2] = -(2.0F * far_plane * near_plane) / range;
  result[3][3] = 0.0F;

  return result;
}

auto CalculateOrthographicMatrix(f32 ortho_width, f32 ortho_height,
                                 f32 near_plane, f32 far_plane) -> math::Mat4 {
  f32 range = far_plane - near_plane;

  math::Mat4 result{};

  result[0][0] = 2.0F / ortho_width;
  result[1][1] = 2.0F / ortho_height;
  result[2][2] = -2.0F / range;
  result[3][2] = -(far_plane + near_plane) / range;
  result[3][3] = 1.0F;

  return result;
}

auto CalculateViewMatrix(const Transform &transform) -> math::Mat4 {
  math::Mat4 result{};

  // Set rotation part
  auto forward = transform.Forward();
  auto right = transform.Right();
  auto local_up = transform.Up();

  auto rot = math::Mat4::Identity();
  rot[0] = math::Vec4(right, 0.0F);
  rot[1] = math::Vec4(local_up, 0.0F);
  rot[2] = math::Vec4(-forward, 0.0F);

  // Set translation part
  auto pos = math::Mat4::Identity();
  pos[3] = math::Vec4{-transform.position, 1.0F};

  result = math::Dot(rot.T(), pos);

  return result;
}

auto Camera::GetProjectionMatrix() const -> math::Mat4 {
  return CalculatePerspectiveMatrix(settings.fov_degrees, settings.aspect_ratio,
                                    settings.near_plane, settings.far_plane);
}

auto Camera::GetViewMatrix() const -> math::Mat4 {
  return CalculateViewMatrix(transform);
}

auto Camera::GetViewProjectionMatrix() const -> math::Mat4 {
  math::Mat4 result{};

  // Set rotation part
  auto forward = transform.Forward();
  auto right = transform.Right();
  auto local_up = transform.Up();

  auto rot = math::Mat4::Identity();
  rot[0] = math::Vec4(right, 0.0F);
  rot[1] = math::Vec4(local_up, 0.0F);
  rot[2] = math::Vec4(-forward, 0.0F);

  // Set translation part
  auto pos = math::Mat4::Identity();
  pos[3] = math::Vec4{-transform.position, 1.0F};

  result = math::Dot(rot.T(), pos);

  return result;
}
} // namespace lumina::core