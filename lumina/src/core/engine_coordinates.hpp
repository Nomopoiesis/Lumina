#pragma once

#include "math/vector.hpp"

namespace lumina::core {

struct EngineCoordinates {
  inline static constexpr math::Vec3 FORWARD{0.0F, 0.0F, -1.0F}; // NOLINT
  inline static constexpr math::Vec3 RIGHT{1.0F, 0.0F, 0.0F};    // NOLINT
  inline static constexpr math::Vec3 UP{0.0F, 1.0F, 0.0F};       // NOLINT
};

} // namespace lumina::core
