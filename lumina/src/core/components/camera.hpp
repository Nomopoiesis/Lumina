#pragma once

#include "lumina_types.hpp"

#include "math/matrix.hpp"
#include "math/trigonometry.hpp"
#include "math/vector.hpp"

namespace lumina::core::components {

enum class ProjectionType : u8 { Perspective = 0, Orthographic = 1 };

struct CameraSettings {
  f32 fov_degrees = 60.0F;         // Field of view in degrees
  f32 aspect_ratio = 16.0F / 9.0F; // Width / Height
  f32 near_plane = 0.1F;           // Near clipping plane
  f32 far_plane = 1000.0F;         // Far clipping plane

  // Orthographic settings (for future use)
  f32 ortho_width = 10.0F;  // Orthographic width
  f32 ortho_height = 10.0F; // Orthographic height
};

class Camera {
public:
  Camera() noexcept = default;
  Camera(const CameraSettings &settings_) noexcept : settings(settings_) {}
  ~Camera() noexcept = default;

  Camera(const Camera &) = default;
  auto operator=(const Camera &) -> Camera & = default;
  Camera(Camera &&) noexcept = default;
  auto operator=(Camera &&) noexcept -> Camera & = default;

  // Projection matrix generation
  [[nodiscard]] auto ToProjectionMatrix() const -> math::Mat4;

  auto Move(const math::Vec3 &direction) -> void;
  auto Rotate(const math::Vec3 &rotation) -> void;

private:
  CameraSettings settings;
};

inline auto CalculatePerspectiveMatrix(f32 fov_degrees, f32 aspect_ratio,
                                       f32 near_plane, f32 far_plane)
    -> math::Mat4 {
  f32 fov_rad = math::DegreesToRadians(fov_degrees);
  f32 tan_half_fov = math::Tan(fov_rad * 0.5F);
  f32 range = far_plane - near_plane;

  math::Mat4 result{};

  result[0][0] = 1.0F / (aspect_ratio * tan_half_fov);
  result[1][1] = 1.0F / tan_half_fov;
  result[2][2] = (-far_plane) / range;
  result[2][3] = -1.0F;
  result[3][2] = -(far_plane * near_plane) / range;
  result[3][3] = 0.0F;

  return result;
}

inline auto CalculateOrthographicMatrix(f32 ortho_width, f32 ortho_height,
                                        f32 near_plane, f32 far_plane)
    -> math::Mat4 {
  f32 range = far_plane - near_plane;

  math::Mat4 result{};

  result[0][0] = 2.0F / ortho_width;
  result[1][1] = 2.0F / ortho_height;
  result[2][2] = -2.0F / range;
  result[3][2] = -(far_plane + near_plane) / range;
  result[3][3] = 1.0F;

  return result;
}

inline auto Camera::ToProjectionMatrix() const -> math::Mat4 {
  return CalculatePerspectiveMatrix(settings.fov_degrees, settings.aspect_ratio,
                                    settings.near_plane, settings.far_plane);
}

} // namespace lumina::core::components
