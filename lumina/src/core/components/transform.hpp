#pragma once

#include "core/engine_coordinates.hpp"
#include "math/quaternion.hpp"
#include "math/vector.hpp"

#include "component_storage.hpp"

namespace lumina::core::components {

// Transform component, this is a "view" into the transform SoA actual layout
class Transform {
public:
  Transform() noexcept = default;
  Transform(const math::Vec3 &position_, const math::Vec3 &rotation_,
            const math::Vec3 &scale_) noexcept
      : position(position_), rotation(rotation_), scale(scale_) {}
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

  [[nodiscard]] auto GetModelMatrix() const -> math::Mat4;

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

inline auto Transform::GetModelMatrix() const -> math::Mat4 {
  auto translation_mat = math::TranslationMatrix(position);
  auto rotation_mat = math::RotationMatrix(rotation);
  auto scale_mat = math::ScaleMatrix(scale);
  return math::Dot(math::Dot(scale_mat, rotation_mat), translation_mat);
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

template <>
class ComponentStorage<Transform> {
public:
  ComponentStorage() = default;
  ComponentStorage(const ComponentStorage &other) = delete;
  ComponentStorage(ComponentStorage &&other) noexcept = delete;
  auto operator=(const ComponentStorage &other) -> ComponentStorage & = delete;
  auto operator=(ComponentStorage &&other) noexcept
      -> ComponentStorage & = delete;
  ~ComponentStorage() = default;

  auto Create(EntityID id, const math::Vec3 &position,
              const math::Vec3 &rotation, const math::Vec3 &scale) -> void;
  auto Create(EntityID id, const Transform &transform) -> void;

  auto Get(EntityID id) -> Transform;
  auto Set(EntityID id, const Transform &component) -> void;

private:
  // SOA layout
  f32 pos_x[1024]{};
  f32 pos_y[1024]{};
  f32 pos_z[1024]{};
  f32 rot_x[1024]{};
  f32 rot_y[1024]{};
  f32 rot_z[1024]{};
  f32 scale_x[1024]{};
  f32 scale_y[1024]{};
  f32 scale_z[1024]{};
  size_t last_index = 0;
  std::unordered_map<EntityID, size_t> entity_to_index;
};

inline auto ComponentStorage<Transform>::Create(EntityID id,
                                                const math::Vec3 &position,
                                                const math::Vec3 &rotation,
                                                const math::Vec3 &scale)
    -> void {
  auto index = last_index++;
  pos_x[index] = position.x;
  pos_y[index] = position.y;
  pos_z[index] = position.z;
  rot_x[index] = rotation.x;
  rot_y[index] = rotation.y;
  rot_z[index] = rotation.z;
  scale_x[index] = scale.x;
  scale_y[index] = scale.y;
  scale_z[index] = scale.z;
  entity_to_index[id] = index;
}

inline auto ComponentStorage<Transform>::Create(EntityID id,
                                                const Transform &transform)
    -> void {
  auto index = last_index++;
  pos_x[index] = transform.position.x;
  pos_y[index] = transform.position.y;
  pos_z[index] = transform.position.z;
  rot_x[index] = transform.rotation.x;
  rot_y[index] = transform.rotation.y;
  rot_z[index] = transform.rotation.z;
  scale_x[index] = transform.scale.x;
  scale_y[index] = transform.scale.y;
  scale_z[index] = transform.scale.z;
  entity_to_index[id] = index;
}

inline auto ComponentStorage<Transform>::Get(EntityID id) -> Transform {
  auto index = entity_to_index[id];
  return {math::Vec3(pos_x[index], pos_y[index], pos_z[index]),
          math::Vec3(rot_x[index], rot_y[index], rot_z[index]),
          math::Vec3(scale_x[index], scale_y[index], scale_z[index])};
}

inline auto ComponentStorage<Transform>::Set(EntityID id,
                                             const Transform &component)
    -> void {
  auto index = entity_to_index[id];
  pos_x[index] = component.position.x;
  pos_y[index] = component.position.y;
  pos_z[index] = component.position.z;
  rot_x[index] = component.rotation.x;
  rot_y[index] = component.rotation.y;
  rot_z[index] = component.rotation.z;
  scale_x[index] = component.scale.x;
  scale_y[index] = component.scale.y;
  scale_z[index] = component.scale.z;
}

} // namespace lumina::core::components
