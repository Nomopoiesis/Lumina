#pragma once

#include "lumina_types.hpp"
#include "math/vector.hpp"

namespace lumina::core::components {

enum class LightType : u8 {
  Point = 0,
  Directional = 1,
  Spot = 2,
};

class LightComponent {
public:
  LightComponent() noexcept = default;
  LightComponent(LightType type_, math::Vec3 color_, f32 intensity_,
                 f32 attenation_radius_) noexcept
      : type(type_), color(color_), intensity(intensity_),
        attenation_radius(attenation_radius_) {}
  LightComponent(const LightComponent &other) noexcept = default;
  LightComponent(LightComponent &&other) noexcept = default;
  auto operator=(const LightComponent &other) noexcept
      -> LightComponent & = default;
  auto operator=(LightComponent &&other) noexcept -> LightComponent & = default;
  ~LightComponent() noexcept = default;

  LightType type = LightType::Point;
  math::Vec3 color = math::Vec3(1.0F, 1.0F, 1.0F);
  f32 intensity = 1.0F;
  f32 attenation_radius = 10.0F;
};

} // namespace lumina::core::components