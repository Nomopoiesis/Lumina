#pragma once

#include "math/linear_algebra.hpp"
#include "math/matrix.hpp"
#include "math/quaternion.hpp"
#include "math/vector.hpp"

#include "core/engine_coordinates.hpp"

namespace lumina::core {

struct Transform {
  math::Vec3 position; // Position in world space (in centimeters)
  math::Vec3 rotation; // Rotation in euler angles (in degrees)
  math::Vec3 scale;    // Scale in local space (1.0f is 100%)

  [[nodiscard]] auto Forward() const -> math::Vec3;
  [[nodiscard]] auto Right() const -> math::Vec3;
  [[nodiscard]] auto Up() const -> math::Vec3;
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

inline auto Transform::Up() const -> math::Vec3 {
  auto forward = Forward();
  auto right = Right();
  return math::Normalize(math::Cross(right, forward));
}

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
  Camera(Transform transform_, const CameraSettings &settings_) noexcept
      : transform(transform_), settings(settings_) {}
  ~Camera() noexcept = default;

  Camera(const Camera &) = default;
  auto operator=(const Camera &) -> Camera & = default;
  Camera(Camera &&) noexcept = default;
  auto operator=(Camera &&) noexcept -> Camera & = default;

  // Projection matrix generation
  [[nodiscard]] auto GetProjectionMatrix() const -> math::Mat4;
  [[nodiscard]] auto GetViewMatrix() const -> math::Mat4;
  [[nodiscard]] auto GetViewProjectionMatrix() const -> math::Mat4;

  auto GetTransform() const -> const Transform & { return transform; }

  auto Move(const math::Vec3 &direction) -> void;
  auto Rotate(const math::Vec3 &rotation) -> void;

private:
  Transform transform;
  CameraSettings settings;
};

auto CalculatePerspectiveMatrix(const CameraSettings &settings) -> math::Mat4;
auto CalculateOrthographicMatrix(const CameraSettings &settings) -> math::Mat4;
auto CalculateViewMatrix(const Transform &transform) -> math::Mat4;
} // namespace lumina::core
