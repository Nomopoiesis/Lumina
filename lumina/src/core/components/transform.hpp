#pragma once

#include "common/lumina_assert.hpp"
#include "common/lumina_check.hpp"
#include "core/engine_coordinates.hpp"
#include "math/quaternion.hpp"
#include "math/vector.hpp"

#include "component_storage.hpp"

#include <vector>

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
class ComponentStorage<Transform> : public IComponentStorage {
public:
  ComponentStorage() = default;
  ComponentStorage(const ComponentStorage &other) = delete;
  ComponentStorage(ComponentStorage &&other) noexcept = delete;
  auto operator=(const ComponentStorage &other) -> ComponentStorage & = delete;
  auto operator=(ComponentStorage &&other) noexcept
      -> ComponentStorage & = delete;
  ~ComponentStorage() override = default;

  auto Create(EntityID id, const math::Vec3 &position,
              const math::Vec3 &rotation, const math::Vec3 &scale,
              Mobility mobility = Mobility::Dynamic) -> void;
  auto Create(EntityID id, const Transform &transform,
              Mobility mobility = Mobility::Dynamic) -> void;

  auto Get(EntityID id) -> Transform;
  auto Set(EntityID id, const Transform &component) -> void;

  auto Destroy(EntityID id) -> void override;
  [[nodiscard]] auto Has(EntityID id) const -> bool override;

  // Transforms are not watched for additions - no system derives work from
  // "a transform appeared" - so this specialisation keeps no log to swap.
  auto ClearAdditions() -> void override {}

private:
  // SOA layout - parallel arrays, one entry per registered entity.
  // Kept dense: Destroy swaps the last entry into the freed slot, so an
  // entity's index is stable only until some other entity is destroyed.
  std::vector<f32> pos_x;
  std::vector<f32> pos_y;
  std::vector<f32> pos_z;
  std::vector<f32> rot_x;
  std::vector<f32> rot_y;
  std::vector<f32> rot_z;
  std::vector<f32> scale_x;
  std::vector<f32> scale_y;
  std::vector<f32> scale_z;
  std::vector<Mobility> mobility_;
  // Reverse of entity_to_index, needed to repoint the entity that gets moved
  // into a freed slot by Destroy's swap-and-pop.
  std::vector<EntityID> index_to_entity;
  std::unordered_map<EntityID, size_t> entity_to_index;
};

inline auto ComponentStorage<Transform>::Create(EntityID id,
                                                const math::Vec3 &position,
                                                const math::Vec3 &rotation,
                                                const math::Vec3 &scale,
                                                Mobility mobility) -> void {
  auto index = pos_x.size();
  pos_x.push_back(position.x);
  pos_y.push_back(position.y);
  pos_z.push_back(position.z);
  rot_x.push_back(rotation.x);
  rot_y.push_back(rotation.y);
  rot_z.push_back(rotation.z);
  scale_x.push_back(scale.x);
  scale_y.push_back(scale.y);
  scale_z.push_back(scale.z);
  mobility_.push_back(mobility);
  index_to_entity.push_back(id);
  entity_to_index[id] = index;
}

inline auto ComponentStorage<Transform>::Create(EntityID id,
                                                const Transform &transform,
                                                Mobility mobility) -> void {
  Create(id, transform.position, transform.rotation, transform.scale, mobility);
}

inline auto ComponentStorage<Transform>::Get(EntityID id) -> Transform {
  auto entry = entity_to_index.find(id);
  ASSERT(entry != entity_to_index.end(),
         "No transform component registered for this entity");
  if (entry == entity_to_index.end()) {
    return {};
  }
  auto index = entry->second;
  return {math::Vec3(pos_x[index], pos_y[index], pos_z[index]),
          math::Vec3(rot_x[index], rot_y[index], rot_z[index]),
          math::Vec3(scale_x[index], scale_y[index], scale_z[index])};
}

inline auto ComponentStorage<Transform>::Set(EntityID id,
                                             const Transform &component)
    -> void {
  auto entry = entity_to_index.find(id);
  ASSERT(entry != entity_to_index.end(),
         "No transform component registered for this entity");
  if (entry == entity_to_index.end()) {
    return;
  }
  LUMINA_CHECK(mobility_[entry->second] == Mobility::Dynamic,
               "Cannot set transform component of static entity");
  auto index = entry->second;
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

inline auto ComponentStorage<Transform>::Destroy(EntityID id) -> void {
  auto entry = entity_to_index.find(id);
  if (entry == entity_to_index.end()) {
    return;
  }

  const auto index = entry->second;
  const auto last = pos_x.size() - 1;

  // Swap-and-pop so the arrays stay dense for linear scans, then repoint the
  // entity that owned the entry we moved.
  if (index != last) {
    pos_x[index] = pos_x[last];
    pos_y[index] = pos_y[last];
    pos_z[index] = pos_z[last];
    rot_x[index] = rot_x[last];
    rot_y[index] = rot_y[last];
    rot_z[index] = rot_z[last];
    scale_x[index] = scale_x[last];
    scale_y[index] = scale_y[last];
    scale_z[index] = scale_z[last];
    mobility_[index] = mobility_[last];

    const auto moved_entity = index_to_entity[last];
    index_to_entity[index] = moved_entity;
    entity_to_index[moved_entity] = index;
  }

  pos_x.pop_back();
  pos_y.pop_back();
  pos_z.pop_back();
  rot_x.pop_back();
  rot_y.pop_back();
  rot_z.pop_back();
  scale_x.pop_back();
  scale_y.pop_back();
  scale_z.pop_back();
  mobility_.pop_back();
  index_to_entity.pop_back();
  entity_to_index.erase(id);
}

inline auto ComponentStorage<Transform>::Has(EntityID id) const -> bool {
  return entity_to_index.contains(id);
}

} // namespace lumina::core::components
