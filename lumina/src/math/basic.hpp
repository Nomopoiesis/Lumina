#pragma once

#include "lumina_types.hpp"
#include <algorithm>
#include <type_traits>

template <typename T>
concept NumericType = std::is_arithmetic_v<T>;

namespace lumina::math {

constexpr auto Clamp(NumericType auto value, NumericType auto min,
                     NumericType auto max) -> NumericType auto {
  return std::clamp(value, min, max);
}

constexpr auto Min(NumericType auto a, NumericType auto b) -> NumericType auto {
  return std::min(a, b);
}

constexpr auto Max(NumericType auto a, NumericType auto b) -> NumericType auto {
  return std::max(a, b);
}

constexpr auto Abs(NumericType auto value) -> NumericType auto {
  return std::abs(value);
}

} // namespace lumina::math
