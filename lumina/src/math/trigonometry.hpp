#pragma once

#include <cmath>
#include <numbers>

#include "lumina_types.hpp"

namespace lumina::math {

constexpr auto PI = std::numbers::pi_v<f32>;
constexpr auto TWO_PI = std::numbers::pi_v<f32> * 2.0F;

inline auto DegreesToRadians(f32 degrees) -> f32 {
  return degrees * (PI / 180.0F);
}

inline auto RadiansToDegrees(f32 radians) -> f32 {
  return radians * (180.0F / PI);
}

inline auto Sin(f32 angle) -> f32 { return std::sin(angle); }

inline auto ASin(f32 value) -> f32 { return std::asin(value); }

inline auto Cos(f32 angle) -> f32 { return std::cos(angle); }

inline auto ACos(f32 value) -> f32 { return std::acos(value); }

inline auto Tan(f32 angle) -> f32 { return std::tan(angle); }

inline auto ATan(f32 Rad) -> f32 { return std::atanf(Rad); }

inline auto ATan2(f32 y, f32 x) -> f32 { return std::atan2f(y, x); }

} // namespace lumina::math
