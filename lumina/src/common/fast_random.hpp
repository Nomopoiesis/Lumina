#pragma once

#include "lumina_types.hpp"

#include <random>

namespace lumina::common::random {

class FastRandom {
public:
  FastRandom(u32 seed) : state(seed) {}

  [[nodiscard]] auto Next() -> u32 {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  }

private:
  u32 state;
};

inline static thread_local FastRandom thd_rnd(std::random_device{}()); // NOLINT

[[nodiscard]] inline auto FastRandom() -> u32 { return thd_rnd.Next(); }

[[nodiscard]] inline auto FastRandom(u32 min, u32 max) -> u32 {
  return min + (FastRandom() % (max - min + 1));
}

} // namespace lumina::common::random