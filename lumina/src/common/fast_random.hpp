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

// Function-local rather than namespace-scope: each thread's generator is then
// built on its first call instead of through a dynamic initializer, and it is
// no longer a symbol callers can reach around FastRandom() to touch.
// `class FastRandom` is spelled out because the enclosing function of the same
// name hides the class name here.
[[nodiscard]] inline auto FastRandom() -> u32 {
  static thread_local class FastRandom thd_rnd(std::random_device{}());
  return thd_rnd.Next();
}

[[nodiscard]] inline auto FastRandom(u32 min, u32 max) -> u32 {
  return min + (FastRandom() % (max - min + 1));
}

} // namespace lumina::common::random
